#include "CelestronOriginSimulator.h"
#include <QApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QNetworkInterface>
#include <QDir>
#include <QFile>
#include <QBuffer>
#include <QImage>
#include <QDebug>
#include <QPainter>
#include <cmath>
#include "CelestronOriginSimulator.h"
#include <QApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QNetworkInterface>
#include <QDir>
#include <QFile>
#include <QBuffer>
#include <QImage>
#include <QDebug>
#include <QPainter>
#include <cmath>

CelestronOriginSimulator::CelestronOriginSimulator(QObject *parent) : QObject(parent) {
    // Initialize core components
    m_telescopeState = new TelescopeState();
    m_commandHandler = new CommandHandler(m_telescopeState, this);
    m_statusSender = new StatusSender(m_telescopeState, this);
    
    // Initialize the dual protocol server
    m_tcpServer = new QTcpServer(this);
    m_udpSocket = new QUdpSocket(this);

    // Initialize DSS integration (replaces HiPS and mosaic creator)
    setupDSSIntegration();
    
    if (m_tcpServer->listen(QHostAddress::Any, SERVER_PORT)) {
        setupConnections();
        setupTimers();
        
        QTimer::singleShot(100, this, &CelestronOriginSimulator::sendBroadcast);
    } else {
        qDebug() << "Failed to start Origin simulator:" << m_tcpServer->errorString();
    }
}

CelestronOriginSimulator::~CelestronOriginSimulator() {
    if (m_tcpServer) {
        m_tcpServer->close();
    }
    qDeleteAll(m_webSocketClients);
}

void CelestronOriginSimulator::setupDSSIntegration() {
    m_dssManager = new DSSFitsManager(this);
    
    qDebug() << "========================================";
    qDebug() << "DSS FITS Manager initialized";
    qDebug() << "Cache directory:" << m_dssManager->getCacheDir();
    
    QList<CachedFitsImage> cached = m_dssManager->getCachedImages();
    qDebug() << "Cached images:" << cached.size();
    
    if (!cached.isEmpty()) {
        qDebug() << "Cache contents:";
        for (const CachedFitsImage& img : cached) {
            qDebug() << QString("  - RA=%1°, Dec=%2°, Size=%3x%4', Fetched=%5")
                        .arg(img.center_ra_deg, 0, 'f', 2)
                        .arg(img.center_dec_deg, 0, 'f', 2)
                        .arg(img.width_arcmin, 0, 'f', 0)
                        .arg(img.height_arcmin, 0, 'f', 0)
                        .arg(img.fetchTime.toString("yyyy-MM-dd hh:mm"));
        }
    }
    
    qint64 cacheSize = m_dssManager->getCacheSize();
    qDebug() << QString("Total cache size: %1 MB").arg(cacheSize / 1024.0 / 1024.0, 0, 'f', 2);
    qDebug() << "========================================";
    
    // Connect signals
    connect(m_dssManager, &DSSFitsManager::imageReady,
            this, &CelestronOriginSimulator::onDSSImageReady);
    connect(m_dssManager, &DSSFitsManager::fetchError,
            this, &CelestronOriginSimulator::onDSSError);
    connect(m_dssManager, &DSSFitsManager::cacheHit,
            this, [](const QString& info) { qDebug() << "📦" << info; });
    connect(m_dssManager, &DSSFitsManager::cacheMiss,
            this, [](const QString& info) { qDebug() << "🌐" << info; });
    
    // Set initial coordinates
    m_telescopeState->ra = m_telescopeState->baseRA;
    m_telescopeState->dec = m_telescopeState->baseDec;
    
    // Fetch initial position
    SkyPosition initialPos = {
        m_telescopeState->ra * 180.0 / M_PI,
        m_telescopeState->dec * 180.0 / M_PI,
        "Initial_Position",
        "Telescope starting position"
    };
    
    fetchDSSImageForPosition(initialPos);
}

void CelestronOriginSimulator::fetchDSSImageForPosition(const SkyPosition& position) {
    qDebug() << QString("🔭 Slew to: RA=%1°, Dec=%2°")
                .arg(position.ra_deg, 0, 'f', 6)
                .arg(position.dec_deg, 0, 'f', 6);
    
    // Manager will automatically determine cache hit/miss
    m_dssManager->fetchImageForPosition(position.ra_deg, position.dec_deg);
}

void CelestronOriginSimulator::onDSSImageReady(const QByteArray& tiffData) {
    qDebug() << "✅ DSS RGB TIFF ready:" << tiffData.size() << "bytes";
    
    // Store TIFF data for HTTP serving
    m_imageData = tiffData;
    
    // Update telescope state
    m_telescopeState->fileLocation = m_telescopeState->getNextImageFile();
    m_telescopeState->imageType = "LIVE";
    m_telescopeState->sequenceNumber++;
    
    // Notify clients
    m_statusSender->sendNewImageReadyToAll();
    
    qDebug() << "📸 Image ready:" << m_telescopeState->fileLocation;
}

void CelestronOriginSimulator::onDSSError(const QString& error) {
    qDebug() << "❌ DSS fetch error:" << error;
    
    // Send warning notification - same format as original
    QJsonObject errorNotification;
    errorNotification["Command"] = "Warning";
    errorNotification["Destination"] = "All";
    errorNotification["Source"] = "ImageServer";
    errorNotification["Type"] = "Notification";
    errorNotification["Message"] = "Image data unavailable: " + error;
    errorNotification["ExpiredAt"] = m_telescopeState->getExpiredAt();
    errorNotification["SequenceID"] = m_telescopeState->getNextSequenceId();
    
    m_statusSender->sendJsonMessageToAll(errorNotification);
}

void CelestronOriginSimulator::updateSlew() {
    static int slewProgress = 0;
    
    slewProgress += 20;
    
    if (slewProgress >= 100) {
        qDebug() << "Before update - RA:" << m_telescopeState->ra << "Dec:" << m_telescopeState->dec;
        qDebug() << "Target RA:" << m_telescopeState->targetRa << "Target Dec:" << m_telescopeState->targetDec;
        
        // Slew complete
        m_telescopeState->isGotoOver = true;
        m_telescopeState->isSlewing = false;
        m_telescopeState->baseRA = m_telescopeState->targetRa;
        m_telescopeState->baseDec = m_telescopeState->targetDec;
        m_telescopeState->ra = m_telescopeState->baseRA;
        m_telescopeState->dec = m_telescopeState->baseDec;
        
        // Stop the timer
        m_slewTimer->stop();
        slewProgress = 0;

        qDebug() << "After update - RA:" << m_telescopeState->ra << "Dec:" << m_telescopeState->dec;
        
        // Update mount status
        m_statusSender->sendMountStatusToAll();

        // Fetch DSS image for new position (replaces HiPS fetch)
        QTimer::singleShot(100, this, [this]() {
            SkyPosition newPos = {
                m_telescopeState->targetRa * 180.0 / M_PI,
                m_telescopeState->targetDec * 180.0 / M_PI,
                "Slew_Target",
                "Position after telescope slew"
            };
            
            qDebug() << "🎯 Using target coordinates for DSS fetch - RA:" 
                     << newPos.ra_deg << "Dec:" << newPos.dec_deg;
            fetchDSSImageForPosition(newPos);
        });
        
        qDebug() << "🎯 Slew complete";
    }
}

void CelestronOriginSimulator::setupConnections() {
    connect(m_tcpServer, &QTcpServer::newConnection, this, &CelestronOriginSimulator::handleNewConnection);
    
    // Connect command handler signals
    connect(m_commandHandler, &CommandHandler::slewStarted, this, [this]() {
        m_slewTimer->start(500);
    });
    
    connect(m_commandHandler, &CommandHandler::imagingStarted, this, [this]() {
        m_imagingTimer->start(1000);
    });

    // Connect initialization signal
    connect(m_commandHandler, &CommandHandler::initializationStarted, this, [this](bool fakeInit) {
        if (fakeInit) {
            // Complete immediately for fake initialization
            QTimer::singleShot(1000, this, &CelestronOriginSimulator::completeInitialization);
        } else {
            // Start the initialization process
            m_initTimer->start(3000); // Update every 3 seconds like real telescope
        }
    });
}

void CelestronOriginSimulator::setupTimers() {
    // Create broadcast timer
    m_broadcastTimer = new QTimer(this);
    connect(m_broadcastTimer, &QTimer::timeout, this, &CelestronOriginSimulator::sendBroadcast);
    m_broadcastTimer->start(BROADCAST_INTERVAL);
    
    // Create update timer for regular status updates
    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, &QTimer::timeout, this, &CelestronOriginSimulator::sendStatusUpdates);
    m_updateTimer->start(1000);
    
    // Create slew timer
    m_slewTimer = new QTimer(this);
    connect(m_slewTimer, &QTimer::timeout, this, &CelestronOriginSimulator::updateSlew);
    
    // Create imaging timer
    m_imagingTimer = new QTimer(this);
    //    connect(m_imagingTimer, &QTimer::timeout, this, &CelestronOriginSimulator::updateImaging);

    // Create initialization timer
    m_initTimer = new QTimer(this);
    m_initTimer->setSingleShot(false);
    connect(m_initTimer, &QTimer::timeout, this, &CelestronOriginSimulator::updateInitialization);
}


void CelestronOriginSimulator::updateInitialization() {
    // Simulate real telescope initialization progression
    m_initUpdateCount++;
    
    // Send status update with progress
    m_statusSender->sendTaskControllerStatusToAll();
    
    // After several updates, set focus position (matches real telescope behavior)
    if (m_initUpdateCount == 5) {
        m_telescopeState->initInfo.positionOfFocus = 18617; // Use value from real trace
    }
    
    // After more updates, find first alignment point
    if (m_initUpdateCount == 10) {
        m_telescopeState->initInfo.numPoints = 1;
        m_telescopeState->initInfo.numPointsRemaining = 1;
        m_telescopeState->initInfo.percentComplete = 50;
    }
    
    // Randomly decide if initialization fails (about 50% chance like real telescope)
    if (m_initUpdateCount < 10 && QRandomGenerator::global()->bounded(100) < 10) {
        failInitialization();
        return;
    }
    
    // Complete the initialization
    if (m_initUpdateCount >= 15) {
        m_telescopeState->initInfo.numPoints = 2;
        m_telescopeState->initInfo.numPointsRemaining = 0;
        m_telescopeState->initInfo.percentComplete = 100;
        completeInitialization();
    }
}

void CelestronOriginSimulator::completeInitialization() {
    // Stop the timer
    m_initTimer->stop();
    m_initUpdateCount = 0;
    
    // Set final status
    m_telescopeState->isInitializing = false;
    m_telescopeState->stage = "COMPLETE";
    m_telescopeState->isReady = true;
    
    // Send completion status
    m_statusSender->sendTaskControllerStatusToAll();
    
    // Wait a moment and transition to IDLE state
    QTimer::singleShot(1000, this, [this]() {
        m_telescopeState->state = "IDLE";
        m_statusSender->sendTaskControllerStatusToAll();
    });
}

void CelestronOriginSimulator::failInitialization() {
    // Stop the timer
    m_initTimer->stop();
    m_initUpdateCount = 0;
    
    // Set failure status
    m_telescopeState->isInitializing = false;
    m_telescopeState->stage = "STOPPED";
    m_telescopeState->isReady = false;
    
    // Send error notification
    QJsonObject errorNotification;
    errorNotification["Command"] = "Error";
    errorNotification["Destination"] = "All";
    errorNotification["ErrorCode"] = -78;
    errorNotification["ErrorMessage"] = "Initialization failed. Please point the scope away from any bright lights; buildings; trees and try again.";
    errorNotification["ExpiredAt"] = m_telescopeState->getExpiredAt();
    errorNotification["Type"] = "Notification";
    m_statusSender->sendJsonMessageToAll(errorNotification);
    
    // Send failure status
    m_statusSender->sendTaskControllerStatusToAll();
}

// Rest of the existing methods remain the same...
void CelestronOriginSimulator::handleNewConnection() {
    QTcpSocket *socket = m_tcpServer->nextPendingConnection();
    
    // Store pending request data
    m_pendingRequests[socket] = QByteArray();
    
    // CRITICAL: Use QueuedConnection to prevent immediate processing conflicts
    connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
        handleIncomingData(socket);
    }, Qt::QueuedConnection);
    
    connect(socket, &QTcpSocket::disconnected, this, [this, socket]() {
        m_pendingRequests.remove(socket);
        socket->deleteLater();
    });
}

// Include the rest of your existing methods here...
// (handleIncomingData, handleWebSocketUpgrade, etc.)
// They remain unchanged from your original implementation

void CelestronOriginSimulator::sendBroadcast() {
    // Prepare the broadcast message
    QString message = QString("Identity:Origin-") + QLatin1String(std::to_string(broadcast_id)) + QString("Z Origin IP Address = %1");
    
    // Get our IP addresses
    QList<QHostAddress> ipAddresses = QNetworkInterface::allAddresses();
    
    for (const QHostAddress &address : ipAddresses) {
        if (address.protocol() == QAbstractSocket::IPv4Protocol && address != QHostAddress::LocalHost) {
            QString broadcastMessage = message.arg(address.toString());
            
            // Send broadcast on all network interfaces
            m_udpSocket->writeDatagram(
                broadcastMessage.toUtf8(),
                QHostAddress::Broadcast,
                BROADCAST_PORT
            );
        }
    }
}


void CelestronOriginSimulator::handleIncomingData(QTcpSocket *socket) {
    QByteArray newData = socket->readAll();
    m_pendingRequests[socket].append(newData);
    
    QByteArray &requestData = m_pendingRequests[socket];
    
    // Look for complete HTTP headers
    int headerEndPos = requestData.indexOf("\r\n\r\n");
    if (headerEndPos == -1) {
        // Headers not complete yet, wait for more data
        if (requestData.size() > 8192) {
            // Too much data without finding headers, disconnect
            socket->disconnectFromHost();
            return;
        }
        return;
    }
    
    // We have complete headers, determine the protocol
    QString request = QString::fromUtf8(requestData.left(headerEndPos));
    QStringList lines = request.split("\r\n");
    
    if (lines.isEmpty()) {
        socket->disconnectFromHost();
        return;
    }
    
    QString requestLine = lines[0];
    QStringList requestParts = requestLine.split(" ");
    
    if (requestParts.size() < 3) {
        socket->disconnectFromHost();
        return;
    }
    
    QString method = requestParts[0];
    QString path = requestParts[1];
    
//     qDebug() << "Origin Protocol Request:" << method << path;
    
    // Check if this is a WebSocket upgrade request
    bool isWebSocketUpgrade = false;
    for (const QString &line : lines) {
        if (line.toLower().contains("upgrade: websocket")) {
            isWebSocketUpgrade = true;
            break;
        }
    }
    
    if (isWebSocketUpgrade && path == "/SmartScope-1.0/mountControlEndpoint") {
        // Handle WebSocket upgrade for telescope control
        handleWebSocketUpgrade(socket, requestData);
    } else if (method == "GET" && path.startsWith("/SmartScope-1.0/dev2/Images/Temp/")) {
        // Handle HTTP image request
        handleHttpImageRequest(socket, path);
    } else if (method == "GET" && path.contains("/SmartScope-1.0/dev2/Images/Astrophotography/")) {
        // Handle HTTP astrophotography image request
        handleHttpAstroImageRequest(socket, path);
    } else {
        // Unknown request
        sendHttpResponse(socket, 404, "text/plain", "Not Found");
    }
    
    // Clear the pending request data
    m_pendingRequests.remove(socket);
}
// CORRECTED FIX: CelestronOriginSimulator.cpp - Proper handshake sequence
// Perform handshake FIRST, then transfer socket ownership

void CelestronOriginSimulator::handleWebSocketUpgrade(QTcpSocket *socket, const QByteArray &requestData) {
//     // qDebug() << "*** STARTING WEBSOCKET UPGRADE PROCESS ***";
//     qDebug() << "Request data size:" << requestData.size();
    
    // Create WebSocketConnection but DON'T disconnect protocol detector yet
    WebSocketConnection *wsConn = new WebSocketConnection(socket, this, false); // false = don't take ownership yet
    
    // FIRST: Perform the handshake using the request data
    if (wsConn->performHandshake(requestData)) {
//         // qDebug() << "*** HANDSHAKE SUCCESSFUL - TRANSFERRING SOCKET OWNERSHIP ***";
        
        // CRITICAL: NOW disconnect the protocol detector since handshake worked
        disconnect(socket, &QTcpSocket::readyRead, this, nullptr);
//         // qDebug() << "*** PROTOCOL DETECTOR DISCONNECTED ***";
        
        // Clear any pending data since we're switching protocols  
        m_pendingRequests.remove(socket);
        
        // NOW let WebSocketConnection take full ownership
        wsConn->takeSocketOwnership();
        
        // Add to our client list
        m_webSocketClients.append(wsConn);
        m_statusSender->addWebSocketClient(wsConn);
        
        // Set up all signal connections for WebSocket handling
        connect(wsConn, &WebSocketConnection::textMessageReceived, 
                this, &CelestronOriginSimulator::processWebSocketCommand);
        
        connect(wsConn, &WebSocketConnection::pingReceived,
                this, &CelestronOriginSimulator::handleWebSocketPing);
        
        connect(wsConn, &WebSocketConnection::pongReceived,
                this, &CelestronOriginSimulator::handleWebSocketPong);
        
        connect(wsConn, &WebSocketConnection::pingTimeout,
                this, &CelestronOriginSimulator::handleWebSocketTimeout);
        
        connect(wsConn, &WebSocketConnection::disconnected, 
                this, &CelestronOriginSimulator::onWebSocketDisconnected);
        
//         qDebug() << "WebSocket connection established for telescope control";
        
        // Send initial status updates after a brief delay
        QTimer::singleShot(1000, this, [this, wsConn]() {
            if (m_webSocketClients.contains(wsConn)) {
                m_statusSender->sendMountStatus(wsConn);
                m_statusSender->sendFocuserStatus(wsConn);
                m_statusSender->sendCameraParams(wsConn);
                m_statusSender->sendDiskStatus(wsConn);
                m_statusSender->sendTaskControllerStatus(wsConn);
                m_statusSender->sendEnvironmentStatus(wsConn);
                m_statusSender->sendDewHeaterStatus(wsConn);
                m_statusSender->sendOrientationStatus(wsConn);
            }
        });
    } else {
//         // qDebug() << "*** HANDSHAKE FAILED - KEEPING PROTOCOL DETECTOR ***";
        
        // Handshake failed, so keep the original protocol detector connected
        // Don't disconnect anything - let it continue as HTTP
        sendHttpResponse(socket, 400, "text/plain", "Bad WebSocket Request");
        
        // Clean up the failed WebSocket connection
        delete wsConn;
    }
}

void CelestronOriginSimulator::handleHttpImageRequest(QTcpSocket *socket, const QString &path) {
//     qDebug() << "Handling HTTP image request for path:" << path;

    sendHttpResponse(socket, 200, "image/jpeg", m_imageData);
}

void CelestronOriginSimulator::handleHttpAstroImageRequest(QTcpSocket *socket, const QString &path) {
    // Parse astrophotography path
    QStringList pathParts = path.split("/");
    if (pathParts.size() < 6) {
        sendHttpResponse(socket, 404, "text/plain", "Invalid path");
        return;
    }
    
    QString directory = pathParts[pathParts.size() - 2];
    QString fileName = pathParts.last();
    QString fullPath = QString("simulator_data/Images/Astrophotography/%1/%2").arg(directory, fileName);
    
    QFile imageFile(fullPath);
    if (!imageFile.exists() || !imageFile.open(QIODevice::ReadOnly)) {
        sendHttpResponse(socket, 404, "text/plain", "Image not found");
        return;
    }
    
    QByteArray imageData = imageFile.readAll();
    imageFile.close();
    
    QString contentType = "image/tiff";
    if (fileName.endsWith(".jpg", Qt::CaseInsensitive)) {
        contentType = "image/jpeg";
    }
    
    sendHttpResponse(socket, 200, contentType, imageData);
//     qDebug() << "Served Origin astrophotography image:" << fileName;
}

void CelestronOriginSimulator::sendHttpResponse(QTcpSocket *socket, int statusCode, 
                     const QString &contentType, const QByteArray &data) {
    QString statusText;
    switch (statusCode) {
        case 200: statusText = "OK"; break;
        case 404: statusText = "Not Found"; break;
        case 400: statusText = "Bad Request"; break;
        case 500: statusText = "Internal Server Error"; break;
        default: statusText = "Unknown"; break;
    }
    
    QString response = QString("HTTP/1.1 %1 %2\r\n").arg(statusCode).arg(statusText);
    response += QString("Content-Type: %1\r\n").arg(contentType);
    response += QString("Content-Length: %1\r\n").arg(data.size());
    response += "Cache-Control: no-cache\r\n";
    response += "Access-Control-Allow-Origin: *\r\n";
    response += "Connection: close\r\n";
    response += "\r\n";
    
    socket->write(response.toUtf8());
    if (!data.isEmpty()) {
        socket->write(data);
    }
    socket->disconnectFromHost();
}

void CelestronOriginSimulator::processWebSocketCommand(const QString &message) {
    WebSocketConnection *wsConn = qobject_cast<WebSocketConnection*>(sender());
    if (!wsConn) return;
    
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject()) {
//         qDebug() << "Received invalid JSON:" << message;
        return;
    }
    
    QJsonObject obj = doc.object();
    
    // Extract common fields
    QString command = obj["Command"].toString();
    QString destination = obj["Destination"].toString();
    QString source = obj["Source"].toString();
    
//     qDebug() << "Received WebSocket command:" << command << "to" << destination << "from" << source;
    
    // Handle status requests directly
    if (command == "GetStatus") {
        int sequenceId = obj["SequenceID"].toInt();
        
        if (destination == "System") {
            m_statusSender->sendSystemVersion(wsConn, sequenceId, source);
        } else if (destination == "Mount") {
            m_statusSender->sendMountStatus(wsConn, sequenceId, source);
        } else if (destination == "Focuser") {
            m_statusSender->sendFocuserStatus(wsConn, sequenceId, source);
        } else if (destination == "TaskController") {
            m_statusSender->sendTaskControllerStatus(wsConn, sequenceId, source);
        } else if (destination == "DewHeater") {
            m_statusSender->sendDewHeaterStatus(wsConn, sequenceId, source);
        } else if (destination == "Environment") {
            m_statusSender->sendEnvironmentStatus(wsConn, sequenceId, source);
        } else if (destination == "OrientationSensor") {
            m_statusSender->sendOrientationStatus(wsConn, sequenceId, source);
        } else if (destination == "Disk") {
            m_statusSender->sendDiskStatus(wsConn, sequenceId, source);
        } else if (destination == "FactoryCalibrationController") {
            m_statusSender->sendCalibrationStatus(wsConn, sequenceId, source);
        }
    } else if (command == "GetVersion") {
        int sequenceId = obj["SequenceID"].toInt();
        m_statusSender->sendSystemVersion(wsConn, sequenceId, source);
    } else if (command == "GetCaptureParameters") {
        int sequenceId = obj["SequenceID"].toInt();
        m_statusSender->sendCameraParams(wsConn, sequenceId, source);
    } else if (command == "GetFilter") {
        int sequenceId = obj["SequenceID"].toInt();
        m_statusSender->sendCameraFilter(wsConn, sequenceId, source);
    } else if (command == "GetModel") {
        int sequenceId = obj["SequenceID"].toInt();
        m_statusSender->sendSystemModel(wsConn, sequenceId, source);
    } else {
        // Process the command through the command handler
        m_commandHandler->processCommand(obj, wsConn);
    }
}

void CelestronOriginSimulator::onWebSocketDisconnected() {
    WebSocketConnection *wsConn = qobject_cast<WebSocketConnection*>(sender());
    if (wsConn) {
        m_webSocketClients.removeAll(wsConn);
        m_statusSender->removeWebSocketClient(wsConn);
        wsConn->deleteLater();
    }
}

// CRITICAL: Fix the status update timing to avoid blocking ping/pong
// Replace sendStatusUpdates method with this non-blocking version:

void CelestronOriginSimulator::sendStatusUpdates() {
    // Update time
    m_telescopeState->dateTime = QDateTime::currentDateTime();
    
    // CRITICAL: Use QueuedConnection to avoid blocking the event loop
    static int updateCounter = 0;
    updateCounter++;
    
    // Send updates in smaller batches to prevent blocking
    if (updateCounter % 1 == 0) {
        // Every second - critical updates only
        QTimer::singleShot(0, this, [this]() {
            m_statusSender->sendMountStatusToAll();
        });
    }
    
    if (updateCounter % 2 == 0) {
        QTimer::singleShot(5, this, [this]() {
            m_statusSender->sendFocuserStatusToAll();
        });
    }
    
    if (updateCounter % 3 == 0) {
        QTimer::singleShot(10, this, [this]() {
            m_statusSender->sendCameraParamsToAll();
            m_telescopeState->sequenceNumber++;
            m_telescopeState->fileLocation = m_telescopeState->getNextImageFile();
            m_statusSender->sendNewImageReadyToAll();
        });
    }
    
    // Less frequent updates with longer delays
    if (updateCounter % 10 == 0) {
        QTimer::singleShot(15, this, [this]() {
            m_statusSender->sendEnvironmentStatusToAll();
            m_statusSender->sendDiskStatusToAll();
        });
    }
    
    if (updateCounter % 15 == 0) {
        QTimer::singleShot(20, this, [this]() {
            m_statusSender->sendDewHeaterStatusToAll();
        });
    }
    
    if (updateCounter % 30 == 0) {
        QTimer::singleShot(25, this, [this]() {
            m_statusSender->sendOrientationStatusToAll();
        });
    }
    
    if (updateCounter % 5 == 0) {
        QTimer::singleShot(30, this, [this]() {
            m_statusSender->sendTaskControllerStatusToAll();
        });
    }
    
    // Reset counter to prevent overflow
    if (updateCounter > 1000) {
        updateCounter = 0;
    }
}

// CRITICAL: Add connection monitoring
// Add this method to monitor connection health:

void CelestronOriginSimulator::checkConnectionHealth() {
//     qDebug() << "Active WebSocket connections:" << m_webSocketClients.size();
    
    for (WebSocketConnection *wsConn : m_webSocketClients) {
        // Check if connection is still alive
        // We don't need to do anything here - the ping/pong mechanism handles it
    }
}

// Add these new slot methods to CelestronOriginSimulator class:

void CelestronOriginSimulator::handleWebSocketPing(const QByteArray &payload) {
    WebSocketConnection *wsConn = qobject_cast<WebSocketConnection*>(sender());
//     qDebug() << "WebSocket ping received from client, payload size:" << payload.size();
    // The WebSocketConnection automatically sends pong, we just log it here
}

void CelestronOriginSimulator::handleWebSocketPong(const QByteArray &payload) {
    WebSocketConnection *wsConn = qobject_cast<WebSocketConnection*>(sender());
//     qDebug() << "WebSocket pong received from client, payload size:" << payload.size();
    // Client responded to our ping successfully
}

void CelestronOriginSimulator::handleWebSocketTimeout() {
    WebSocketConnection *wsConn = qobject_cast<WebSocketConnection*>(sender());
//     qDebug() << "WebSocket ping timeout occurred - client not responding";
    
    if (wsConn && m_webSocketClients.contains(wsConn)) {
        // Remove from active clients but don't delete yet - let disconnected signal handle cleanup
        m_statusSender->removeWebSocketClient(wsConn);
    }
}


// Method 1: Save to QByteArray as JPEG/PNG (most common)
QByteArray saveImageToByteArray(const QImage& image, const QString& format = "JPEG", int quality = 95) {
    QByteArray byteArray;
    QBuffer buffer(&byteArray);
    buffer.open(QIODevice::WriteOnly);
    
    // Save image to buffer in specified format
    bool success = image.save(&buffer, format.toUtf8().data(), quality);
    
    if (success) {
        qDebug() << QString("Image saved to buffer: %1 bytes, format: %2")
                    .arg(byteArray.size()).arg(format);
    } else {
        qDebug() << "Failed to save image to buffer";
    }
    
    return byteArray;
}

// And call it in the constructor:
// CelestronOriginSimulator::CelestronOriginSimulator(QObject *parent) : QObject(parent) {
//     // ... existing initialization ...
//     setupHipsIntegration();
//     setupMosaicCreator();  // Add this line
//     // ... rest of initialization ...
// }
