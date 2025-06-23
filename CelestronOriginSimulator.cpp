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
    
    if (m_tcpServer->listen(QHostAddress::Any, SERVER_PORT)) {
        qDebug() << "Origin simulator listening on port" << SERVER_PORT;
        qDebug() << "WebSocket: ws://localhost/SmartScope-1.0/mountControlEndpoint";
        qDebug() << "HTTP Images: http://localhost/SmartScope-1.0/dev2/Images/Temp/";
        
        setupConnections();
        setupTimers();
        
        // Ensure temp directory exists and create dummy images
        QDir().mkpath("simulator_data/Images/Temp");
        createDummyImages();
        
        // First broadcast immediately
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

void CelestronOriginSimulator::setupConnections() {
    connect(m_tcpServer, &QTcpServer::newConnection, this, &CelestronOriginSimulator::handleNewConnection);
    
    // Connect command handler signals
    connect(m_commandHandler, &CommandHandler::slewStarted, this, [this]() {
        m_slewTimer->start(500);
    });
    
    connect(m_commandHandler, &CommandHandler::imagingStarted, this, [this]() {
        m_imagingTimer->start(1000);
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
}

void CelestronOriginSimulator::handleNewConnection() {
    QTcpSocket *socket = m_tcpServer->nextPendingConnection();
    
    // Store pending request data
    m_pendingRequests[socket] = QByteArray();
    
    connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
        handleIncomingData(socket);
    });
    
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
    
    qDebug() << "Origin Protocol Request:" << method << path;
    
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

void CelestronOriginSimulator::handleWebSocketUpgrade(QTcpSocket *socket, const QByteArray &requestData) {
    WebSocketConnection *wsConn = new WebSocketConnection(socket, this);
    
    if (wsConn->performHandshake(requestData)) {
        m_webSocketClients.append(wsConn);
        m_statusSender->addWebSocketClient(wsConn);
        
        connect(wsConn, &WebSocketConnection::textMessageReceived, 
                this, &CelestronOriginSimulator::processWebSocketCommand);
        connect(wsConn, &WebSocketConnection::pingReceived,
                this, &CelestronOriginSimulator::handleWebSocketPing);
        connect(wsConn, &WebSocketConnection::disconnected, 
                this, &CelestronOriginSimulator::onWebSocketDisconnected);
        
        qDebug() << "WebSocket connection established for telescope control";
        
        // Send initial status updates
        QTimer::singleShot(500, this, [this, wsConn]() {
            m_statusSender->sendMountStatus(wsConn);
            m_statusSender->sendFocuserStatus(wsConn);
            m_statusSender->sendCameraParams(wsConn);
            m_statusSender->sendDiskStatus(wsConn);
            m_statusSender->sendTaskControllerStatus(wsConn);
            m_statusSender->sendEnvironmentStatus(wsConn);
            m_statusSender->sendDewHeaterStatus(wsConn);
            m_statusSender->sendOrientationStatus(wsConn);
        });
    } else {
        sendHttpResponse(socket, 400, "text/plain", "Bad WebSocket Request");
    }
}

void CelestronOriginSimulator::handleHttpImageRequest(QTcpSocket *socket, const QString &path) {
    qDebug() << "Handling HTTP image request for path:" << path;

    // Extract filename from path: /SmartScope-1.0/dev2/Images/Temp/0.jpg -> 0.jpg
    QString fileName = path.split("/").last();

    // Remove any query parameters if present
    if (fileName.contains("?")) {
        fileName = fileName.split("?").first();
    }

    qDebug() << "Extracted filename:" << fileName;

    QString fullPath = QString("/Users/jonathan/OriginSimulator/simulator_data/Images/Temp/%1").arg(fileName);
    qDebug() << "Looking for file at:" << fullPath;

    QFile imageFile(fullPath);
    if (!imageFile.exists()) {
        qDebug() << "Image file does not exist:" << fullPath;

        // List what files actually exist in the directory
        QDir tempDir("simulator_data/Images/Temp");
        if (tempDir.exists()) {
            qDebug() << "Available files in simulator_data/Images/Temp:";
            QStringList files = tempDir.entryList(QDir::Files);
            for (const QString &file : files) {
                qDebug() << "  " << file;
            }
        } else {
            qDebug() << "Directory simulator_data/Images/Temp does not exist!";
        }

        sendHttpResponse(socket, 404, "text/plain", "Image not found");
        return;
    }

    if (!imageFile.open(QIODevice::ReadOnly)) {
        qDebug() << "Failed to open image file:" << fullPath;
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

    qDebug() << "Serving image:" << fileName << "(" << imageData.size() << "bytes) with content-type:" << contentType;

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
    qDebug() << "Served Origin astrophotography image:" << fileName;
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
        qDebug() << "Received invalid JSON:" << message;
        return;
    }
    
    QJsonObject obj = doc.object();
    
    // Extract common fields
    QString command = obj["Command"].toString();
    QString destination = obj["Destination"].toString();
    QString source = obj["Source"].toString();
    
    qDebug() << "Received WebSocket command:" << command << "to" << destination << "from" << source;
    
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

void CelestronOriginSimulator::handleWebSocketPing(const QByteArray &payload) {
    qDebug() << "WebSocket ping received with payload:" << payload;
    // The WebSocketConnection handles sending the pong automatically
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
            
            qDebug() << "Sent broadcast:" << broadcastMessage;
        }
    }
}

void CelestronOriginSimulator::sendStatusUpdates() {
    // Update time
    m_telescopeState->dateTime = QDateTime::currentDateTime();
    
    // Send regular status updates to all WebSocket clients
    m_statusSender->sendMountStatusToAll();
    
    // Only send image update every 3 seconds to avoid flooding
    static int imageCounter = 0;
    if (++imageCounter % 3 == 0) {
        // Simulate a new image every 3 seconds
        m_telescopeState->sequenceNumber++;
        m_telescopeState->fileLocation = QString("Images/Temp/%1.jpg").arg(m_telescopeState->sequenceNumber % 10);
        
        // Slightly adjust RA and Dec to simulate movement
        m_telescopeState->ra += 0.0001;
        m_telescopeState->dec += 0.00001;
        
        m_statusSender->sendNewImageReadyToAll();
    }
    
    // Send other status updates less frequently
    static int statusCounter = 0;
    if (++statusCounter % 5 == 0) {
        m_statusSender->sendFocuserStatusToAll();
        m_statusSender->sendCameraParamsToAll();
        
        if (statusCounter % 10 == 0) {
            m_statusSender->sendEnvironmentStatusToAll();
            m_statusSender->sendDiskStatusToAll();
            m_statusSender->sendDewHeaterStatusToAll();
            m_statusSender->sendOrientationStatusToAll();
        }
    }
}

void CelestronOriginSimulator::updateSlew() {
    static int slewProgress = 0;
    
    // Simulate slew progress
    slewProgress += 20;  // 20% progress per 500ms
    
    if (slewProgress >= 100) {
        // Slew complete
        m_telescopeState->isGotoOver = true;
        m_telescopeState->isSlewing = false;
        m_telescopeState->ra = m_telescopeState->targetRa;
        m_telescopeState->dec = m_telescopeState->targetDec;
        
        // Stop the timer
        m_slewTimer->stop();
        slewProgress = 0;
        
        // Update mount status
        m_statusSender->sendMountStatusToAll();
        
        qDebug() << "Slew complete";
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
        
        qDebug() << "Imaging complete";
    }
}

void CelestronOriginSimulator::createDummyImages() {
    // Create directory structure
    QString tempDir = "simulator_data/Images/Temp";
    QString astroDir = "simulator_data/Images/Astrophotography";

    if (!QDir().mkpath(tempDir)) {
        qDebug() << "Failed to create directory:" << tempDir;
        return;
    }

    if (!QDir().mkpath(astroDir)) {
        qDebug() << "Failed to create directory:" << astroDir;
        return;
    }

    qDebug() << "Creating dummy images in:" << tempDir;

    // Create 10 dummy images (0.jpg to 9.jpg)
    for (int i = 0; i < 10; ++i) {
        // Create a simple image with text
        QImage image(800, 600, QImage::Format_RGB888);
        image.fill(Qt::black);

        // Draw some "stars"
        QPainter painter(&image);
        painter.setPen(Qt::white);
        painter.setFont(QFont("Arial", 20));
        painter.drawText(QRect(50, 50, 700, 100), 
                        Qt::AlignCenter, 
                        QString("Celestron Origin Simulator - Image %1").arg(i));

        // Draw some random stars
        painter.setPen(Qt::white);
        painter.setBrush(Qt::white);
        for (int j = 0; j < 200; ++j) {
            int x = qrand() % (image.width() - 10);
            int y = qrand() % (image.height() - 10);
            int size = (qrand() % 3) + 1;
            painter.drawEllipse(x, y, size, size);
        }

        // Add timestamp
        painter.setPen(Qt::lightGray);
        painter.setFont(QFont("Arial", 12));
        painter.drawText(QRect(10, image.height() - 30, 400, 20), 
                        Qt::AlignLeft, 
                        QString("Created: %1").arg(QDateTime::currentDateTime().toString()));

        painter.end();

        // Save the image
        QString fileName = QString("%1/%2.jpg").arg(tempDir).arg(i);

        if (image.save(fileName, "JPEG", 90)) {
            qDebug() << "Successfully created dummy image:" << fileName;

            // Verify the file exists and get its size
            QFileInfo fileInfo(fileName);
            if (fileInfo.exists()) {
                qDebug() << "  File size:" << fileInfo.size() << "bytes";
            } else {
                qDebug() << "  ERROR: File was not created successfully!";
            }
        } else {
            qDebug() << "Failed to save image:" << fileName;
        }
    }

    // Create subdirectory for each astrophotography dir
    for (const QString &dir : m_telescopeState->astrophotographyDirs) {
        QString dirPath = QString("%1/%2").arg(astroDir).arg(dir);
        if (!QDir().mkpath(dirPath)) {
            qDebug() << "Failed to create astrophotography directory:" << dirPath;
            continue;
        }

        // Create a stacked master image
        QImage masterImage(1024, 768, QImage::Format_RGB888);
        masterImage.fill(Qt::black);

        // Draw some "stars" and target name
        QPainter painter(&masterImage);
        painter.setPen(Qt::white);
        painter.setFont(QFont("Arial", 24));
        painter.drawText(QRect(50, 50, 900, 100), 
                        Qt::AlignCenter, 
                        QString("Stacked Image: %1").arg(dir));

        // Draw some random stars with galaxy-like pattern
        painter.setPen(Qt::white);
        painter.setBrush(Qt::white);
        for (int j = 0; j < 1000; ++j) {
            // Create a spiral galaxy pattern
            double angle = (qrand() % 360) * M_PI / 180.0;
            double distance = (qrand() % 300) + 50;
            double xOffset = masterImage.width() / 2 + cos(angle) * distance;
            double yOffset = masterImage.height() / 2 + sin(angle) * distance;

            int x = xOffset + (qrand() % 20) - 10;
            int y = yOffset + (qrand() % 20) - 10;

            // Skip if outside image
            if (x < 0 || x >= masterImage.width() || 
                y < 0 || y >= masterImage.height()) {
                continue;
            }

            int brightness = qrand() % 200 + 55;
            painter.setPen(QColor(brightness, brightness, brightness));
            painter.setBrush(QColor(brightness, brightness, brightness));

            int size = (qrand() % 4) + 1;
            painter.drawEllipse(x, y, size, size);
        }

        painter.end();

        // Save the image
        QString fileName = QString("%1/FinalStackedMaster.tiff").arg(dirPath);
        if (masterImage.save(fileName, "TIFF")) {
            qDebug() << "Successfully created dummy stacked image:" << fileName;
        } else {
            qDebug() << "Failed to save stacked image:" << fileName;
        }

        // Create some individual frame images
        for (int frame = 1; frame <= 3; ++frame) {
            QString frameFileName = QString("%1/frame_%2.jpg").arg(dirPath).arg(frame);
            if (masterImage.save(frameFileName, "JPEG", 90)) {
                qDebug() << "Created frame image:" << frameFileName;
            }
        }
    }

    // List all created files for verification
    qDebug() << "Verification - Files in" << tempDir << ":";
    QDir tempDirObj(tempDir);
    QStringList tempFiles = tempDirObj.entryList(QDir::Files);
    for (const QString &file : tempFiles) {
        QFileInfo fileInfo(tempDirObj.absoluteFilePath(file));
        qDebug() << "  " << file << "(" << fileInfo.size() << "bytes)";
    }
}
