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
#define qrand rand

CelestronOriginSimulator::CelestronOriginSimulator(QObject *parent) : QObject(parent) {
    // Initialize core components
    m_telescopeState = new TelescopeState();
    m_commandHandler = new CommandHandler(m_telescopeState, this);
    m_statusSender = new StatusSender(m_telescopeState, this);
    
    // Initialize the dual protocol server
    m_tcpServer = new QTcpServer(this);
    m_udpSocket = new QUdpSocket(this);

    setupRubinIntegration();
    
    if (m_tcpServer->listen(QHostAddress::Any, SERVER_PORT)) {
//         qDebug() << "Origin simulator listening on port" << SERVER_PORT;
//         qDebug() << "WebSocket: ws://localhost/SmartScope-1.0/mountControlEndpoint";
//         qDebug() << "HTTP Images: http://localhost/SmartScope-1.0/dev2/Images/Temp/";
        
        setupConnections();
        setupTimers();
        
        // Ensure temp directory exists and create dummy images
        QDir().mkpath("simulator_data/Images/Temp");
        createDummyImages();
        
        // First broadcast immediately
        QTimer::singleShot(100, this, &CelestronOriginSimulator::sendBroadcast);
    } else {
//         qDebug() << "Failed to start Origin simulator:" << m_tcpServer->errorString();
    }
}

CelestronOriginSimulator::~CelestronOriginSimulator() {
    if (m_tcpServer) {
        m_tcpServer->close();
    }
    qDeleteAll(m_webSocketClients);
}


void CelestronOriginSimulator::setupRubinIntegration() {
    m_rubinClient = new RubinHipsClient(this);
    
    // Set up Rubin image directory to integrate with existing simulator structure
    QString homeDir = QDir::homePath();
    QString appSupportDir = QDir(homeDir).absoluteFilePath("Library/Application Support/OriginSimulator");
    QString rubinDir = QDir(appSupportDir).absoluteFilePath("Images/Rubin");
    m_rubinClient->setImageDirectory(rubinDir);
 
    // Connect Rubin client callbacks to telescope behavior (using lambdas instead of signals/slots)
    m_rubinClient->onImageReady = [this](const QString& filename) {
        onRubinImageReady(filename);
    };
    
    m_rubinClient->onTilesAvailable = [this](const QStringList& filenames) {
        onRubinTilesAvailable(filenames);
    };
    
    m_rubinClient->onFetchError = [this](const QString& error_message) {
        onRubinFetchError(error_message);
    };
    
    m_rubinClient->onTilesFetched = [this](int successful, int total) {
//         qDebug() << "Rubin fetch complete:" << successful << "/" << total << "tiles";
    };
    
    m_rubinClient->onFetchProgress = [this](int completed, int total) {
//         qDebug() << "Rubin fetch progress:" << completed << "/" << total;
    };
        
//     qDebug() << "Rubin Observatory integration initialized";
//     qDebug() << "Rubin images will be saved to:" << rubinDir;
}

// Add these new slot methods to CelestronOriginSimulator.cpp:

void CelestronOriginSimulator::onRubinImageReady(const QString& filename) {
//     qDebug() << "Rubin Observatory image ready:" << filename;
    
    // Extract just the filename for the telescope's image system
    QFileInfo fileInfo(filename);
    QString relativePath = QString("Images/Rubin/%1").arg(fileInfo.fileName());
    
    // Update telescope state with new image location
    m_telescopeState->fileLocation = relativePath;
    m_telescopeState->imageType = "RUBIN_HIPS";
    m_telescopeState->sequenceNumber++;
    
    // Notify all connected clients about the new image
    m_statusSender->sendNewImageReadyToAll();
    
//     qDebug() << "Telescope updated with Rubin image:" << relativePath;
}

void CelestronOriginSimulator::onRubinTilesAvailable(const QStringList& filenames) {
//     qDebug() << "Rubin Observatory tiles available:" << filenames.size();
    
    if (!filenames.isEmpty()) {
        // Use the first available tile
        onRubinImageReady(filenames.first());
        
        // Log all available files
        for (const QString& file : filenames) {
            QFileInfo info(file);
//             qDebug() << "  Available:" << info.fileName() << "(" << info.size() << "bytes)";
        }
    }
}

void CelestronOriginSimulator::onRubinFetchError(const QString& error_message) {
//     qDebug() << "Rubin Observatory fetch error:" << error_message;
    
    // Could send error notification to clients if desired
    QJsonObject errorNotification;
    errorNotification["Command"] = "Warning";
    errorNotification["Destination"] = "All";
    errorNotification["Source"] = "RubinImageServer";
    errorNotification["Type"] = "Notification";
    errorNotification["Message"] = "Rubin Observatory data unavailable: " + error_message;
    errorNotification["ExpiredAt"] = m_telescopeState->getExpiredAt();
    errorNotification["SequenceID"] = m_telescopeState->getNextSequenceId();
    
    m_statusSender->sendJsonMessageToAll(errorNotification);
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

void CelestronOriginSimulator::updateInitialization() {
    // Simulate real telescope initialization progression
    m_initUpdateCount++;
    
    // Send status update with progress
    m_statusSender->sendTaskControllerStatusToAll();
    
    // After several updates, set focus position (matches real telescope behavior)
    if (m_initUpdateCount == 5) {
        m_telescopeState->initInfo.positionOfFocus = 18617; // Use value from real trace
//         qDebug() << "Initialization found focus position:" << m_telescopeState->initInfo.positionOfFocus;
    }
    
    // After more updates, find first alignment point
    if (m_initUpdateCount == 10) {
        m_telescopeState->initInfo.numPoints = 1;
        m_telescopeState->initInfo.numPointsRemaining = 1;
        m_telescopeState->initInfo.percentComplete = 50;
//         qDebug() << "Initialization found first alignment point";
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
//         qDebug() << "Initialization complete - telescope ready";
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
    
//     qDebug() << "Initialization failed with error -78";
}

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

    // Extract filename from path: /SmartScope-1.0/dev2/Images/Temp/0.jpg -> 0.jpg
    QString fileName = path.split("/").last();

    // Remove any query parameters if present
    if (fileName.contains("?")) {
        fileName = fileName.split("?").first();
    }

//     qDebug() << "Extracted filename:" << fileName;

    QString fullPath = QString("/Users/jonathan/OriginSimulator/simulator_data/Images/Temp/%1").arg(fileName);
//     qDebug() << "Looking for file at:" << fullPath;

    QFile imageFile(fullPath);
    if (!imageFile.exists()) {
//         qDebug() << "Image file does not exist:" << fullPath;

        // List what files actually exist in the directory
        QDir tempDir("simulator_data/Images/Temp");
        if (tempDir.exists()) {
//             qDebug() << "Available files in simulator_data/Images/Temp:";
            QStringList files = tempDir.entryList(QDir::Files);
            for (const QString &file : files) {
//                 qDebug() << "  " << file;
            }
        } else {
//             qDebug() << "Directory simulator_data/Images/Temp does not exist!";
        }

        sendHttpResponse(socket, 404, "text/plain", "Image not found");
        return;
    }

    if (!imageFile.open(QIODevice::ReadOnly)) {
//         qDebug() << "Failed to open image file:" << fullPath;
        sendHttpResponse(socket, 500, "text/plain", "Failed to read image");
        return;
    }

    QByteArray imageData = imageFile.readAll();
    imageFile.close();

    // Determine content type
    QString contentType = "image/jpeg";
    if (fileName.endsWith(".png", Qt::CaseInsensitive)) {
        contentType = "image/png";
    } else if (fileName.endsWith(".tiff", Qt::CaseInsensitive) || fileName.endsWith(".tif", Qt::CaseInsensitive)) {
        contentType = "image/tiff";
    }

//     qDebug() << "Serving image:" << fileName << "(" << imageData.size() << "bytes) with content-type:" << contentType;

    sendHttpResponse(socket, 200, contentType, imageData);
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

void CelestronOriginSimulator::sendBroadcast() {
    // Prepare the broadcast message
    QString message = QString("Origin IP Address: %1 Identity: Origin140020 Version: 1.1.4248");
    
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
            
//             qDebug() << "Sent broadcast:" << broadcastMessage;
        }
    }
}

void CelestronOriginSimulator::updateSlew() {
    static int slewProgress = 0;
    
    // Simulate slew progress
    slewProgress += 20;  // 20% progress per 500ms
    
    if (slewProgress >= 100) {
        // ADD THESE DEBUG LINES BEFORE COORDINATE UPDATE:
//         // qDebug() << "*** SLEW COMPLETING ***";
//         qDebug() << "Before update - RA:" << m_telescopeState->ra << "Dec:" << m_telescopeState->dec;
//         qDebug() << "Target RA:" << m_telescopeState->targetRa << "Target Dec:" << m_telescopeState->targetDec;
        
        // Slew complete
        m_telescopeState->isGotoOver = true;
        m_telescopeState->isSlewing = false;
        m_telescopeState->ra = m_telescopeState->targetRa;
        m_telescopeState->dec = m_telescopeState->targetDec;
        
        // Stop the timer
        m_slewTimer->stop();
        slewProgress = 0;

// 	qDebug() << "After update - RA:" << m_telescopeState->ra << "Dec:" << m_telescopeState->dec;
		
        // Update mount status
        m_statusSender->sendMountStatusToAll();

	// Add delay to ensure coordinates are fully updated
	QTimer::singleShot(100, this, [this]() {
	    if (m_rubinClient) {
// 		qDebug() << "🎯 Slew complete - fetching Rubin Observatory data for new position";
		m_rubinClient->fetchTilesForCurrentPointing(m_telescopeState);
	    }
	});

        // TRIGGER RUBIN FETCH FOR NEW POSITION
        if (m_rubinClient) {
//             qDebug() << "🎯 Slew complete - fetching Rubin Observatory data for new position";
            m_rubinClient->fetchTilesForCurrentPointing(m_telescopeState);
        }
        
//         qDebug() << "🎯 Slew complete";
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
        
//         qDebug() << "Imaging complete";
    }
}

// Enhanced image creation method using macOS Application Support directory
// Replace the createDummyImages method with this version:

void CelestronOriginSimulator::createDummyImages() {
    // Create Application Support directory structure (macOS standard)
    QString homeDir = QDir::homePath();
    QString appSupportDir = QDir(homeDir).absoluteFilePath("Library/Application Support/OriginSimulator");
    QString tempDir = QDir(appSupportDir).absoluteFilePath("Images/Temp");
    QString astroDir = QDir(appSupportDir).absoluteFilePath("Images/Astrophotography");

//     qDebug() << "Home directory:" << homeDir;
//     qDebug() << "Application Support directory:" << appSupportDir;
//     qDebug() << "Live images directory:" << tempDir;
//     qDebug() << "Astrophotography directory:" << astroDir;

    // Create the directory structure
    if (!QDir().mkpath(tempDir)) {
//         qDebug() << "Failed to create directory:" << tempDir;
        return;
    }

    if (!QDir().mkpath(astroDir)) {
//         qDebug() << "Failed to create directory:" << astroDir;
        return;
    }

    // Store the absolute paths for later use in HTTP serving
    m_absoluteTempDir = tempDir;
    m_absoluteAstroDir = astroDir;

//     qDebug() << "Creating realistic astronomy images in:" << tempDir;

    // Create 10 realistic live view images (0.jpg to 9.jpg)
    for (int i = 0; i < 10; ++i) {
        // Create realistic astronomy image (800x600 like real Origin camera)
        QImage image(800, 600, QImage::Format_RGB888);
        image.fill(QColor(5, 5, 10)); // Dark sky background

        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);

        // Create realistic star field
        for (int star = 0; star < 500; ++star) {
            int x = qrand() % image.width();
            int y = qrand() % image.height();
            
            // Vary star brightness and size realistically
            int brightness = 50 + (qrand() % 205); // 50-255 brightness
            int size = 1 + (qrand() % 3); // 1-3 pixel stars mostly
            
            // Occasional bright star
            if (qrand() % 20 == 0) {
                brightness = 200 + (qrand() % 55); // Very bright
                size = 2 + (qrand() % 2); // Larger
            }
            
            QColor starColor(brightness, brightness, brightness - (qrand() % 30));
            painter.setPen(starColor);
            painter.setBrush(starColor);
            
            // Draw star with slight gaussian blur effect
            painter.drawEllipse(x-size/2, y-size/2, size, size);
            
            // Add slight diffraction spikes for brighter stars
            if (brightness > 180) {
                painter.setPen(QColor(brightness/3, brightness/3, brightness/3));
                painter.drawLine(x-size*2, y, x+size*2, y); // Horizontal spike
                painter.drawLine(x, y-size*2, x, y+size*2); // Vertical spike
            }
        }

        // Add some realistic nebulosity (faint background glow)
        for (int nebula = 0; nebula < 3; ++nebula) {
            int centerX = qrand() % image.width();
            int centerY = qrand() % image.height();
            int radius = 20 + (qrand() % 60);
            
            QRadialGradient gradient(centerX, centerY, radius);
            gradient.setColorAt(0, QColor(20, 15, 25, 100)); // Faint purple core
            gradient.setColorAt(0.5, QColor(10, 8, 15, 50));
            gradient.setColorAt(1, QColor(5, 5, 10, 0)); // Fade to background
            
            painter.setBrush(QBrush(gradient));
            painter.setPen(Qt::NoPen);
            painter.drawEllipse(centerX - radius, centerY - radius, radius*2, radius*2);
        }

        // Add some realistic noise (like real CCD cameras)
        for (int noise = 0; noise < 2000; ++noise) {
            int x = qrand() % image.width();
            int y = qrand() % image.height();
            QColor pixel = image.pixelColor(x, y);
            int variation = (qrand() % 10) - 5; // ±5 noise
            pixel.setRed(qBound(0, pixel.red() + variation, 255));
            pixel.setGreen(qBound(0, pixel.green() + variation, 255));
            pixel.setBlue(qBound(0, pixel.blue() + variation, 255));
            image.setPixelColor(x, y, pixel);
        }

        // Add crosshairs (like real telescope live view)
        painter.setPen(QColor(100, 100, 100, 150));
        int centerX = image.width() / 2;
        int centerY = image.height() / 2;
        
        // Crosshair lines
        painter.drawLine(centerX - 20, centerY, centerX + 20, centerY);
        painter.drawLine(centerX, centerY - 20, centerX, centerY + 20);
        
        // Corner markers
        painter.drawLine(10, 10, 30, 10);
        painter.drawLine(10, 10, 10, 30);
        painter.drawLine(image.width()-30, 10, image.width()-10, 10);
        painter.drawLine(image.width()-10, 10, image.width()-10, 30);

        // Add realistic timestamp and info overlay
        painter.setPen(QColor(200, 200, 200, 180));
        painter.setFont(QFont("Consolas", 8));
        
        QString timeStr = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
        QString infoStr = QString("ISO:%1 EXP:%2s BIN:%3x%3")
                         .arg(m_telescopeState->iso)
                         .arg(m_telescopeState->exposure, 0, 'f', 1)
                         .arg(m_telescopeState->binning);
        
        painter.drawText(10, image.height() - 25, timeStr);
        painter.drawText(10, image.height() - 10, infoStr);
        
        // Add frame number
        painter.drawText(image.width() - 50, 20, QString("Frame %1").arg(i));

        painter.end();

        // Save the image with absolute path
        QString fileName = QDir(tempDir).absoluteFilePath(QString("%1.jpg").arg(i));

        if (image.save(fileName, "JPEG", 95)) {
//             qDebug() << "Successfully created realistic astronomy image:" << fileName;

            // Verify the file exists and get its size
            QFileInfo fileInfo(fileName);
            if (fileInfo.exists()) {
//                 qDebug() << "  File size:" << fileInfo.size() << "bytes";
            } else {
//                 qDebug() << "  ERROR: File was not created successfully!";
            }
        } else {
//             qDebug() << "Failed to save image:" << fileName;
        }
    }

    // Create realistic astrophotography directories and images
    for (const QString &target : m_telescopeState->astrophotographyDirs) {
        QString dirPath = QDir(astroDir).absoluteFilePath(target);
        if (!QDir().mkpath(dirPath)) {
//             qDebug() << "Failed to create astrophotography directory:" << dirPath;
            continue;
        }

        // Create a realistic deep-space object image
        QImage deepSpaceImage(1024, 768, QImage::Format_RGB888);
        deepSpaceImage.fill(QColor(2, 2, 5)); // Very dark space background

        QPainter painter(&deepSpaceImage);
        painter.setRenderHint(QPainter::Antialiasing);

        // Create appropriate deep-space object based on target name
        if (target.contains("Galaxy", Qt::CaseInsensitive)) {
            // Create spiral galaxy structure
            int centerX = deepSpaceImage.width() / 2;
            int centerY = deepSpaceImage.height() / 2;
            
            // Galaxy core
            QRadialGradient coreGradient(centerX, centerY, 40);
            coreGradient.setColorAt(0, QColor(255, 220, 180, 200)); // Bright core
            coreGradient.setColorAt(0.3, QColor(200, 170, 140, 150));
            coreGradient.setColorAt(0.7, QColor(100, 90, 80, 100));
            coreGradient.setColorAt(1, QColor(50, 45, 40, 50));
            
            painter.setBrush(QBrush(coreGradient));
            painter.setPen(Qt::NoPen);
            painter.drawEllipse(centerX - 40, centerY - 40, 80, 80);
            
            // Spiral arms
            for (int arm = 0; arm < 2; ++arm) {
                for (double t = 0; t < 6 * M_PI; t += 0.1) {
                    double spiral_a = 10;
                    double x = centerX + (spiral_a * t * cos(t + arm * M_PI));
                    double y = centerY + (spiral_a * t * sin(t + arm * M_PI) * 0.6); // Flattened
                    
                    if (x >= 0 && x < deepSpaceImage.width() && y >= 0 && y < deepSpaceImage.height()) {
                        int brightness = 100 - (t * 15);
                        if (brightness > 20) {
                            painter.setPen(QColor(brightness, brightness * 0.9, brightness * 0.8, 100));
                            painter.drawPoint(x, y);
                        }
                    }
                }
            }
        } else if (target.contains("Nebula", Qt::CaseInsensitive)) {
            // Create nebula structure
            int centerX = deepSpaceImage.width() / 2;
            int centerY = deepSpaceImage.height() / 2;
            
            // Multiple overlapping colored gas clouds
            QColor nebulaColors[] = {
                QColor(255, 100, 100, 80), // H-alpha red
                QColor(100, 255, 150, 60), // OIII green
                QColor(150, 150, 255, 70)  // Blue reflection
            };
            
            for (int cloud = 0; cloud < 3; ++cloud) {
                int cloudX = centerX + (qrand() % 200) - 100;
                int cloudY = centerY + (qrand() % 150) - 75;
                int radius = 60 + (qrand() % 100);
                
                QRadialGradient cloudGradient(cloudX, cloudY, radius);
                cloudGradient.setColorAt(0, nebulaColors[cloud]);
                cloudGradient.setColorAt(0.5, QColor(nebulaColors[cloud].red(), 
                                                   nebulaColors[cloud].green(), 
                                                   nebulaColors[cloud].blue(), 40));
                cloudGradient.setColorAt(1, QColor(nebulaColors[cloud].red(), 
                                                 nebulaColors[cloud].green(), 
                                                 nebulaColors[cloud].blue(), 0));
                
                painter.setBrush(QBrush(cloudGradient));
                painter.setPen(Qt::NoPen);
                painter.drawEllipse(cloudX - radius, cloudY - radius, radius*2, radius*2);
            }
        }

        // Add realistic star field for deep space image
        for (int star = 0; star < 1000; ++star) {
            int x = qrand() % deepSpaceImage.width();
            int y = qrand() % deepSpaceImage.height();
            
            int brightness = 30 + (qrand() % 225);
            int size = 1;
            
            // Occasional bright stars
            if (qrand() % 50 == 0) {
                brightness = 200 + (qrand() % 55);
                size = 2 + (qrand() % 2);
            }
            
            QColor starColor(brightness, brightness * 0.95, brightness * 0.9);
            painter.setPen(starColor);
            painter.setBrush(starColor);
            painter.drawEllipse(x-size/2, y-size/2, size, size);
        }

        painter.end();

        // Save the deep space image with absolute path
        QString masterFileName = QDir(dirPath).absoluteFilePath("FinalStackedMaster.tiff");
        if (deepSpaceImage.save(masterFileName, "TIFF")) {
//             qDebug() << "Successfully created realistic deep-space image:" << masterFileName;
        } else {
//             qDebug() << "Failed to save deep-space image:" << masterFileName;
        }

        // Create some individual frame images (slightly different each time)
        for (int frame = 1; frame <= 5; ++frame) {
            QString frameFileName = QDir(dirPath).absoluteFilePath(QString("frame_%1.jpg").arg(frame));
            
            // Add slight variations to simulate individual exposures
            QImage frameImage = deepSpaceImage.copy();
            QPainter framePainter(&frameImage);
            
            // Add some random noise and slight offset
            for (int noise = 0; noise < 1000; ++noise) {
                int x = qrand() % frameImage.width();
                int y = qrand() % frameImage.height();
                QColor pixel = frameImage.pixelColor(x, y);
                int variation = (qrand() % 20) - 10;
                pixel.setRed(qBound(0, pixel.red() + variation, 255));
                pixel.setGreen(qBound(0, pixel.green() + variation, 255));
                pixel.setBlue(qBound(0, pixel.blue() + variation, 255));
                frameImage.setPixelColor(x, y, pixel);
            }
            
            framePainter.end();
            
            if (frameImage.save(frameFileName, "JPEG", 90)) {
//                 qDebug() << "Created frame image:" << frameFileName;
            }
        }
    }

    // List all created files for verification
//     qDebug() << "Verification - Files in" << tempDir << ":";
    QDir tempDirObj(tempDir);
    QStringList tempFiles = tempDirObj.entryList(QDir::Files);
    for (const QString &file : tempFiles) {
        QFileInfo fileInfo(tempDirObj.absoluteFilePath(file));
//         qDebug() << "  " << file << "(" << fileInfo.size() << "bytes)";
    }
    
    // Print cleanup information
//     qDebug() << "Images created in Application Support directory:" << appSupportDir;
//     qDebug() << "Location: ~/Library/Application Support/OriginSimulator/";
//     qDebug() << "To view in Finder: open ~/Library/Application\\ Support/OriginSimulator/";
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
