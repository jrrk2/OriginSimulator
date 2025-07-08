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

    // Initialize headless mosaic creator
    m_mosaicCreator = new EnhancedMosaicCreator(this); // Headless mode
    m_mosaicInProgress = false;
    
    // Connect mosaic completion signal
    connect(m_mosaicCreator, &EnhancedMosaicCreator::mosaicComplete, 
            this, &CelestronOriginSimulator::onMosaicComplete);
    
    if (false) qDebug() << "Headless Enhanced Mosaic Creator initialized";
    
    setupHipsIntegration();  // Changed from setupRubinIntegration
    
    if (m_tcpServer->listen(QHostAddress::Any, SERVER_PORT)) {
        setupConnections();
        setupTimers();
        
        // First broadcast immediately
        QTimer::singleShot(100, this, &CelestronOriginSimulator::sendBroadcast);
    } else {
        if (false) qDebug() << "Failed to start Origin simulator:" << m_tcpServer->errorString();
    }
}

CelestronOriginSimulator::~CelestronOriginSimulator() {
    if (m_tcpServer) {
        m_tcpServer->close();
    }
    qDeleteAll(m_webSocketClients);
}

void CelestronOriginSimulator::setupHipsIntegration() {
    m_hipsClient = new ProperHipsClient(this);
    
    // Set up HiPS image directory to integrate with existing simulator structure
    QString homeDir = QDir::homePath();
    QString appSupportDir = QDir(homeDir).absoluteFilePath("Library/Application Support/OriginSimulator");
    QString hipsDir = QDir(appSupportDir).absoluteFilePath("Images/HiPS");
    
    // Create the HiPS directory
    QDir().mkpath(hipsDir);
    
    // Connect ProperHipsClient signals to our slots
    connect(m_hipsClient, &ProperHipsClient::testingComplete, 
            this, &CelestronOriginSimulator::onHipsTestingComplete);
    
    if (false) qDebug() << "ProperHips integration initialized";
    if (false) qDebug() << "HiPS images will be saved to:" << hipsDir;

    // Set initial coordinates
    m_telescopeState->ra = m_telescopeState->baseRA;
    m_telescopeState->dec = m_telescopeState->baseDec;

    // Start with initial position fetch
    SkyPosition initialPos = {
        m_telescopeState->ra * 180.0 / M_PI,
        m_telescopeState->dec * 180.0 / M_PI,
        "Initial_Position",
        "Telescope starting position"
    };
    
    fetchHipsImagesForPosition(initialPos);
}

void CelestronOriginSimulator::fetchHipsImagesForPosition(const SkyPosition& position) {
    if (false) qDebug() << QString("Fetching HiPS images for position: RA=%1°, Dec=%2°")
                .arg(position.ra_deg, 0, 'f', 6)
                .arg(position.dec_deg, 0, 'f', 6);
    
    // Use the best available survey
    QString bestSurvey = getBestAvailableSurvey();
    if (bestSurvey.isEmpty()) {
        if (false) qDebug() << "No working surveys available";
        return;
    }
    
    // Test the survey at this position
    m_hipsClient->testSurveyAtPosition(bestSurvey, position);
    
    generateCurrentSkyImage();
}

QString CelestronOriginSimulator::getBestAvailableSurvey() const {
    // Get working surveys from previous tests, or use defaults
    QStringList workingSurveys = m_hipsClient->getWorkingSurveys();
    
    if (workingSurveys.isEmpty()) {
        // Default to known working surveys
        QStringList defaultSurveys = {"DSS2_Color", "2MASS_Color", "2MASS_J"};
        return defaultSurveys.first();
    }
    
    // Prefer color surveys over single-band
    QStringList preferredOrder = {"DSS2_Color", "2MASS_Color", "Mellinger_Color", 
                                  "2MASS_J", "DSS2_Red", "Gaia_DR3"};
    
    for (const QString& preferred : preferredOrder) {
        if (workingSurveys.contains(preferred)) {
            return preferred;
        }
    }
    
    return workingSurveys.first();
}

// Slot implementations for HiPS integration
void CelestronOriginSimulator::onHipsImageReady(const QString& filename) {
    if (false) qDebug() << "HiPS Observatory image ready:" << filename;
    
    // Extract just the filename for the telescope's image system
    QFileInfo fileInfo(filename);
    QString relativePath = QString("Images/HiPS/%1").arg(fileInfo.fileName());
    
    // Update telescope state with new image location
    m_telescopeState->fileLocation = relativePath;
    m_telescopeState->imageType = "HIPS_IMAGE";
    m_telescopeState->sequenceNumber++;

    // Notify all connected clients about the new image
    m_statusSender->sendNewImageReadyToAll();
    
    if (true) qDebug() << "Telescope updated with HiPS image:" << relativePath;
}

void CelestronOriginSimulator::onHipsTilesAvailable(const QStringList& filenames) {
    if (false) qDebug() << "HiPS Observatory tiles available:" << filenames.size();
    
    if (!filenames.isEmpty()) {
        // Use the first available tile
        onHipsImageReady(filenames.first());
        
        // Log all available files
        for (const QString& file : filenames) {
            QFileInfo info(file);
            if (false) qDebug() << "  Available:" << info.fileName() << "(" << info.size() << "bytes)";
        }
    }
}

void CelestronOriginSimulator::onHipsFetchError(const QString& error_message) {
    if (false) qDebug() << "HiPS Observatory fetch error:" << error_message;
    
    // Send error notification to clients
    QJsonObject errorNotification;
    errorNotification["Command"] = "Warning";
    errorNotification["Destination"] = "All";
    errorNotification["Source"] = "HipsImageServer";
    errorNotification["Type"] = "Notification";
    errorNotification["Message"] = "HiPS Observatory data unavailable: " + error_message;
    errorNotification["ExpiredAt"] = m_telescopeState->getExpiredAt();
    errorNotification["SequenceID"] = m_telescopeState->getNextSequenceId();
    
    m_statusSender->sendJsonMessageToAll(errorNotification);
}

void CelestronOriginSimulator::onHipsTestingComplete() {
    if (false) qDebug() << "HiPS testing completed";
    
    // Print summary of results
    m_hipsClient->printSummary();
    
    // Save results for debugging
    m_hipsClient->saveResults("hips_test_results.csv");
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
    connect(m_imagingTimer, &QTimer::timeout, this, &CelestronOriginSimulator::updateImaging);

    // Create initialization timer
    m_initTimer = new QTimer(this);
    m_initTimer->setSingleShot(false);
    connect(m_initTimer, &QTimer::timeout, this, &CelestronOriginSimulator::updateInitialization);
}

void CelestronOriginSimulator::updateSlew() {
    static int slewProgress = 0;
    
    // Simulate slew progress
    slewProgress += 20;  // 20% progress per 500ms
    
    if (slewProgress >= 100) {
        if (false) qDebug() << "Before update - RA:" << m_telescopeState->ra << "Dec:" << m_telescopeState->dec;
        if (false) qDebug() << "Target RA:" << m_telescopeState->targetRa << "Target Dec:" << m_telescopeState->targetDec;
        
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

        if (false) qDebug() << "After update - RA:" << m_telescopeState->ra << "Dec:" << m_telescopeState->dec;
        
        // Update mount status
        m_statusSender->sendMountStatusToAll();

        // Add delay to ensure coordinates are fully updated
        QTimer::singleShot(100, this, [this]() {
            if (m_hipsClient) {
                if (false) qDebug() << "🎯 Slew complete - fetching HiPS data for new position";
                
                // Convert telescope coordinates to SkyPosition
                SkyPosition newPos = {
                    m_telescopeState->targetRa * 180.0 / M_PI,  // Convert radians to degrees
                    m_telescopeState->targetDec * 180.0 / M_PI,
                    "Slew_Target",
                    "Position after telescope slew"
                };
                
                if (false) qDebug() << "🎯 Using target coordinates for HiPS fetch - RA:" << newPos.ra_deg << "Dec:" << newPos.dec_deg;
                fetchHipsImagesForPosition(newPos);
            }
        });
        
        if (false) qDebug() << "🎯 Slew complete";
    }
}

void CelestronOriginSimulator::updateImaging() {
    // Decrement imaging time
    m_telescopeState->imagingTimeLeft--;

    // Send a new image notification
    m_statusSender->sendNewImageReadyToAll();
    
    if (m_telescopeState->imagingTimeLeft <= 0) {
        // Imaging complete
        m_telescopeState->isImaging = false;
        m_imagingTimer->stop();
        
        if (false) qDebug() << "Imaging complete";
    }
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
            if (false) qDebug() << broadcastMessage;
            
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
    
//     if (false) qDebug() << "Origin Protocol Request:" << method << path;
    
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
//     // if (false) qDebug() << "*** STARTING WEBSOCKET UPGRADE PROCESS ***";
//     if (false) qDebug() << "Request data size:" << requestData.size();
    
    // Create WebSocketConnection but DON'T disconnect protocol detector yet
    WebSocketConnection *wsConn = new WebSocketConnection(socket, this, false); // false = don't take ownership yet
    
    // FIRST: Perform the handshake using the request data
    if (wsConn->performHandshake(requestData)) {
//         // if (false) qDebug() << "*** HANDSHAKE SUCCESSFUL - TRANSFERRING SOCKET OWNERSHIP ***";
        
        // CRITICAL: NOW disconnect the protocol detector since handshake worked
        disconnect(socket, &QTcpSocket::readyRead, this, nullptr);
//         // if (false) qDebug() << "*** PROTOCOL DETECTOR DISCONNECTED ***";
        
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
        
//         if (false) qDebug() << "WebSocket connection established for telescope control";
        
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
//         // if (false) qDebug() << "*** HANDSHAKE FAILED - KEEPING PROTOCOL DETECTOR ***";
        
        // Handshake failed, so keep the original protocol detector connected
        // Don't disconnect anything - let it continue as HTTP
        sendHttpResponse(socket, 400, "text/plain", "Bad WebSocket Request");
        
        // Clean up the failed WebSocket connection
        delete wsConn;
    }
}

void CelestronOriginSimulator::handleHttpImageRequest(QTcpSocket *socket, const QString &path) {
    if (false) qDebug() << "Handling HTTP image request for path:" << path;

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
//     if (false) qDebug() << "Served Origin astrophotography image:" << fileName;
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
//         if (false) qDebug() << "Received invalid JSON:" << message;
        return;
    }
    
    QJsonObject obj = doc.object();
    
    // Extract common fields
    QString command = obj["Command"].toString();
    QString destination = obj["Destination"].toString();
    QString source = obj["Source"].toString();
    
//     if (false) qDebug() << "Received WebSocket command:" << command << "to" << destination << "from" << source;
    
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
//     if (false) qDebug() << "Active WebSocket connections:" << m_webSocketClients.size();
    
    for (WebSocketConnection *wsConn : m_webSocketClients) {
        // Check if connection is still alive
        // We don't need to do anything here - the ping/pong mechanism handles it
    }
}

// Add these new slot methods to CelestronOriginSimulator class:

void CelestronOriginSimulator::handleWebSocketPing(const QByteArray &payload) {
    WebSocketConnection *wsConn = qobject_cast<WebSocketConnection*>(sender());
//     if (false) qDebug() << "WebSocket ping received from client, payload size:" << payload.size();
    // The WebSocketConnection automatically sends pong, we just log it here
}

void CelestronOriginSimulator::handleWebSocketPong(const QByteArray &payload) {
    WebSocketConnection *wsConn = qobject_cast<WebSocketConnection*>(sender());
//     if (false) qDebug() << "WebSocket pong received from client, payload size:" << payload.size();
    // Client responded to our ping successfully
}

void CelestronOriginSimulator::handleWebSocketTimeout() {
    WebSocketConnection *wsConn = qobject_cast<WebSocketConnection*>(sender());
//     if (false) qDebug() << "WebSocket ping timeout occurred - client not responding";
    
    if (wsConn && m_webSocketClients.contains(wsConn)) {
        // Remove from active clients but don't delete yet - let disconnected signal handle cleanup
        m_statusSender->removeWebSocketClient(wsConn);
    }
}

void CelestronOriginSimulator::generateCurrentSkyImage() {
    // Skip if already generating
    if (m_mosaicInProgress) {
        if (false) qDebug() << "Mosaic generation already in progress, skipping";
        return;
    }
    
    // Get current telescope pointing
    double ra_rad = m_telescopeState->ra;
    double dec_rad = m_telescopeState->dec;
    double ra_deg = ra_rad * 180.0 / M_PI;
    double dec_deg = dec_rad * 180.0 / M_PI;
    
    if (false) qDebug() << QString("Generating enhanced mosaic for RA=%1°, Dec=%2°")
                .arg(ra_deg, 0, 'f', 6)
                .arg(dec_deg, 0, 'f', 6);
    
    // Create SkyPosition using the existing structure
    SkyPosition currentPosition = {
        ra_deg,
        dec_deg,
        QString("Live_Image_%1").arg(m_telescopeState->imageCounter),
        QString("Real-time telescope position at %1").arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
    };
    
    // Convert coordinates to string format for the mosaic creator
    double ra_hours = ra_deg / 15.0;
    int ra_h = static_cast<int>(ra_hours);
    int ra_m = static_cast<int>((ra_hours - ra_h) * 60);
    double ra_s = ((ra_hours - ra_h) * 60 - ra_m) * 60;
    
    bool dec_negative = dec_deg < 0;
    double abs_dec = std::abs(dec_deg);
    int dec_d = static_cast<int>(abs_dec);
    int dec_m = static_cast<int>((abs_dec - dec_d) * 60);
    double dec_s = ((abs_dec - dec_d) * 60 - dec_m) * 60;
    
    QString raText = QString("%1h%2m%3s")
                     .arg(ra_h).arg(ra_m, 2, 10, QChar('0')).arg(ra_s, 0, 'f', 1);
    
    QString decText = QString("%1%2d%3m%4s")
                      .arg(dec_negative ? "-" : "+")
                      .arg(dec_d).arg(dec_m, 2, 10, QChar('0')).arg(dec_s, 0, 'f', 1);
    
    if (false) qDebug() << QString("Converted coordinates: %1, %2").arg(raText).arg(decText);
    
    // Set coordinates in the headless mosaic creator
    m_mosaicCreator->setCustomCoordinates(raText, decText, currentPosition.name);
    
    // Start mosaic creation (this will use the existing 3x3 tile logic)
    m_mosaicInProgress = true;
    m_mosaicCreator->createCustomMosaic(currentPosition);
    
    if (false) qDebug() << "Enhanced mosaic generation started...";
}

// Method 1: Save to QByteArray as JPEG/PNG (most common)
QByteArray saveImageToByteArray(const QImage& image, const QString& format = "JPEG", int quality = 95) {
    QByteArray byteArray;
    QBuffer buffer(&byteArray);
    buffer.open(QIODevice::WriteOnly);
    
    // Save image to buffer in specified format
    bool success = image.save(&buffer, format.toUtf8().data(), quality);
    
    if (success) {
        if (false) qDebug() << QString("Image saved to buffer: %1 bytes, format: %2")
                    .arg(byteArray.size()).arg(format);
    } else {
        if (false) qDebug() << "Failed to save image to buffer";
    }
    
    return byteArray;
}

void CelestronOriginSimulator::onMosaicComplete(const QImage& mosaic) {
    if (false) qDebug() << "Enhanced mosaic complete, processing for telescope image";
    
    if (mosaic.isNull()) {
        if (false) qDebug() << "Received null mosaic image";
        m_mosaicInProgress = false;
        return;
    }
    
    if (false) qDebug() << QString("Received mosaic: %1x%2 pixels").arg(mosaic.width()).arg(mosaic.height());
    
    // Resize to telescope camera resolution (800x600) - Origin camera specs
    QImage telescopeImage = mosaic.scaled(800, 600, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    
    // Fill any letterbox areas with black (if aspect ratios don't match)
    if (telescopeImage.width() != 800 || telescopeImage.height() != 600) {
        QImage finalImage(800, 600, QImage::Format_RGB888);
        finalImage.fill(Qt::black);
        
        QPainter painter(&finalImage);
        int x = (800 - telescopeImage.width()) / 2;
        int y = (600 - telescopeImage.height()) / 2;
        painter.drawImage(x, y, telescopeImage);
        painter.end();
        
        telescopeImage = finalImage;
    }
    
    // Add telescope-specific overlay
    QPainter painter(&telescopeImage);
    addTelescopeOverlay(painter, telescopeImage);
    painter.end();
    
    m_imageData = saveImageToByteArray(telescopeImage, "JPEG", 95);
  
    if (true) qDebug() << QString("Updated mosaic: %1x%2 pixels").arg(telescopeImage.width()).arg(telescopeImage.height());

    m_mosaicInProgress = false;
}

void CelestronOriginSimulator::addTelescopeOverlay(QPainter& painter, const QImage& image) {
    // Add crosshairs at center (where the exact coordinates are)
    painter.setPen(QPen(Qt::yellow, 2));
    int centerX = image.width() / 2;
    int centerY = image.height() / 2;
    
    painter.drawLine(centerX - 30, centerY, centerX + 30, centerY);
    painter.drawLine(centerX, centerY - 30, centerX, centerY + 30);
    
    // Add coordinate info (top left)
    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 10, QFont::Bold));
    
    double ra_deg = m_telescopeState->ra * 180.0 / M_PI;
    double dec_deg = m_telescopeState->dec * 180.0 / M_PI;
    
    QString coordText = QString("RA: %1° Dec: %2°")
                       .arg(ra_deg, 0, 'f', 3)
                       .arg(dec_deg, 0, 'f', 3);
    
    painter.drawText(10, 20, coordText);
    
    // Add timestamp (bottom left)
    QString timeText = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss UTC");
    painter.drawText(10, image.height() - 25, timeText);
    
    // Add exposure info (bottom left, second line)
    QString exposureText = QString("ISO:%1 EXP:%2s BIN:%3x%3")
                          .arg(m_telescopeState->iso)
                          .arg(m_telescopeState->exposure, 0, 'f', 1)
                          .arg(m_telescopeState->binning);
    painter.drawText(10, image.height() - 10, exposureText);
    
    // Add "REAL HiPS DATA" label (top right)
    painter.setPen(Qt::green);
    painter.setFont(QFont("Arial", 8, QFont::Bold));
    painter.drawText(image.width() - 120, 20, "REAL HiPS DATA");
    
    // Add precision indicator (top right, second line)
    painter.setPen(Qt::cyan);
    painter.setFont(QFont("Arial", 7));
    painter.drawText(image.width() - 120, 35, "1.61\"/pixel precision");
    
    // Add frame number (bottom right)
    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 8));
    QString frameText = QString("Frame %1").arg(m_telescopeState->imageCounter % 10);
    painter.drawText(image.width() - 80, image.height() - 10, frameText);
    
    // Add center marker with coordinate precision
    painter.setPen(QPen(Qt::red, 1));
    painter.drawEllipse(centerX - 2, centerY - 2, 4, 4);
    
    // Add FOV indicator (corner markers)
    painter.setPen(QPen(Qt::gray, 1));
    painter.drawLine(5, 5, 25, 5);      // Top left
    painter.drawLine(5, 5, 5, 25);
    painter.drawLine(image.width()-25, 5, image.width()-5, 5);  // Top right
    painter.drawLine(image.width()-5, 5, image.width()-5, 25);
    painter.drawLine(5, image.height()-25, 5, image.height()-5);  // Bottom left
    painter.drawLine(5, image.height()-5, 25, image.height()-5);
    painter.drawLine(image.width()-5, image.height()-25, image.width()-5, image.height()-5);  // Bottom right
    painter.drawLine(image.width()-25, image.height()-5, image.width()-5, image.height()-5);
}

// And call it in the constructor:
// CelestronOriginSimulator::CelestronOriginSimulator(QObject *parent) : QObject(parent) {
//     // ... existing initialization ...
//     setupHipsIntegration();
//     setupMosaicCreator();  // Add this line
//     // ... rest of initialization ...
// }
