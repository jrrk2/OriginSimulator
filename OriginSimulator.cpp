// OriginSimulator.cpp
// Complete Celestron Origin telescope simulator with dual HTTP/WebSocket protocol on port 80

#include <QApplication>
#include <QTcpServer>
#include <QTcpSocket>
#include <QWebSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QDateTime>
#include <QUdpSocket>
#include <QNetworkInterface>
#include <QDir>
#include <QFile>
#include <QBuffer>
#include <QImage>
#include <QDebug>
#include <QPainter>
#include <QCryptographicHash>
#include <cmath>

// Constants
const QString SERVER_NAME = "CelestronOriginSimulator";
const int SERVER_PORT = 80;
const int BROADCAST_PORT = 55555;
const int BROADCAST_INTERVAL = 5000; // milliseconds

class TelescopeState {
public:
    // Mount data
    QString batteryLevel = "HIGH";
    double batteryVoltage = 10.38;
    QString chargerStatus = "CHARGING";
    QDateTime dateTime = QDateTime::currentDateTime();
    QString timeZone = "Europe/London";
    double latitude = 0.5907141501744784;  // Radians (approximately Cambridge, UK)
    double longitude = -2.065392832281757; // Radians
    bool isAligned = false;
    bool isGotoOver = true;
    bool isTracking = false;
    int numAlignRefs = 0;
    double enc0 = 0.0;
    double enc1 = 0.0;
    
    // Camera data
    int binning = 1;
    int bitDepth = 24;
    double colorBBalance = 120.0;
    double colorGBalance = 58.0;
    double colorRBalance = 78.0;
    double exposure = 0.03;
    int iso = 100;
    int offset = 0;
    
    // Focuser data
    int backlash = 255;
    int calibrationLowerLimit = 1975;
    int calibrationUpperLimit = 37527;
    bool isCalibrationComplete = true;
    bool isMoveToOver = true;
    bool needAutoFocus = false;
    int percentageCalibrationComplete = 100;
    int position = 18386;
    bool requiresCalibration = false;
    double velocity = 0.0;
    
    // Environment data
    double ambientTemperature = 12.14;
    double cameraTemperature = 21.7;
    bool cpuFanOn = true;
    double cpuTemperature = 40.4;
    double dewPoint = 6.29;
    double frontCellTemperature = 11.35;
    double humidity = 67.0;
    bool otaFanOn = true;
    bool recalibrating = false;
    
    // Image data
    QString fileLocation = "Images/Temp/0.jpg";
    QString imageType = "LIVE";
    double dec = 0.973655;
    double ra = 3.83883;
    double orientation = 0.003215;
    double fovX = 0.021893731343283578;
    double fovY = 0.014672238805970147;
    
    // Disk data
    qint64 capacity = 58281033728;
    qint64 freeBytes = 52024094720;
    QString level = "OK";
    
    // Dew Heater data
    int aggression = 5;
    double heaterLevel = 0.0;
    double manualPowerLevel = 0.0;
    QString mode = "Auto";
    
    // Orientation data
    int altitude = 28;
    
    // Task Controller data
    bool isReady = false;
    QString stage = "IN_PROGRESS";
    QString state = "IDLE";
    
    // System version data
    QString versionNumber = "1.1.4248";
    QString versionString = "1.1.4248\n (C++ = 09-04-2024 18:19, Java = 09-04-2024 18:19)";
    
    // Image sequence
    int sequenceNumber = 1;
    
    // Commands being executed
    bool isSlewing = false;
    bool isImaging = false;
    double targetRa = 0.0;
    double targetDec = 0.0;
    int imagingTimeLeft = 0;
    
    // Available directories for download
    QStringList astrophotographyDirs = {
        "(4)_Vesta_05-05-25_22_30_25",
        "Bode's_Nebulae_05-05-25_22_00_53",
        "Messier_3_05-05-25_21_51_52",
        "Messier_101_05-03-25_22_33_50",
        "Whirlpool_Galaxy_05-03-25_21_58_46",
        "Jupiter_05-03-25_21_55_57"
    };
};

class WebSocketConnection : public QObject {
    Q_OBJECT
    
public:
    WebSocketConnection(QTcpSocket *socket, QObject *parent = nullptr) 
        : QObject(parent), m_socket(socket), m_handshakeComplete(false) {
        connect(m_socket, &QTcpSocket::readyRead, this, &WebSocketConnection::handleData);
        connect(m_socket, &QTcpSocket::disconnected, this, &WebSocketConnection::disconnected);
    }
    
    void sendTextMessage(const QString &message) {
        if (!m_handshakeComplete || !m_socket) return;
        
        // Simple WebSocket frame creation (text frame)
        QByteArray data = message.toUtf8();
        QByteArray frame;
        
        // Frame format: FIN(1) + RSV(3) + Opcode(4) + MASK(1) + Payload Length(7+) + Payload
        frame.append(0x81); // FIN=1, Opcode=1 (text)
        
        if (data.size() < 126) {
            frame.append(data.size());
        } else if (data.size() < 65536) {
            frame.append(126);
            frame.append((data.size() >> 8) & 0xFF);
            frame.append(data.size() & 0xFF);
        } else {
            frame.append(127);
            for (int i = 7; i >= 0; --i) {
                frame.append((data.size() >> (i * 8)) & 0xFF);
            }
        }
        
        frame.append(data);
        m_socket->write(frame);
    }
    
    bool performHandshake(const QByteArray &requestData) {
        QString request = QString::fromUtf8(requestData);
        QStringList lines = request.split("\r\n");
        
        QString webSocketKey;
        for (const QString &line : lines) {
            if (line.startsWith("Sec-WebSocket-Key:", Qt::CaseInsensitive)) {
                webSocketKey = line.mid(18).trimmed();
                break;
            }
        }
        
        if (webSocketKey.isEmpty()) {
            return false;
        }
        
        // Generate WebSocket accept key
        QString acceptKey = webSocketKey + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        QByteArray acceptHash = QCryptographicHash::hash(acceptKey.toUtf8(), QCryptographicHash::Sha1);
        QString acceptValue = acceptHash.toBase64();
        
        // Send WebSocket handshake response
        QString response = "HTTP/1.1 101 Switching Protocols\r\n";
        response += "Upgrade: websocket\r\n";
        response += "Connection: Upgrade\r\n";
        response += QString("Sec-WebSocket-Accept: %1\r\n").arg(acceptValue);
        response += "\r\n";
        
        m_socket->write(response.toUtf8());
        m_handshakeComplete = true;
        
        return true;
    }

signals:
    void textMessageReceived(const QString &message);
    void disconnected();

private slots:
    void handleData() {
        if (!m_handshakeComplete) return;
        
        QByteArray data = m_socket->readAll();
        // Simple WebSocket frame parsing (assumes single frame, unmasked)
        
        if (data.size() < 2) return;
        
        quint8 firstByte = data[0];
        quint8 secondByte = data[1];
        
        bool fin = (firstByte & 0x80) != 0;
        quint8 opcode = firstByte & 0x0F;
        bool masked = (secondByte & 0x80) != 0;
        quint64 payloadLength = secondByte & 0x7F;
        
        int headerSize = 2;
        
        if (payloadLength == 126) {
            if (data.size() < 4) return;
            payloadLength = (quint8(data[2]) << 8) | quint8(data[3]);
            headerSize = 4;
        } else if (payloadLength == 127) {
            if (data.size() < 10) return;
            payloadLength = 0;
            for (int i = 0; i < 8; ++i) {
                payloadLength = (payloadLength << 8) | quint8(data[2 + i]);
            }
            headerSize = 10;
        }
        
        if (masked) {
            headerSize += 4; // Mask key
        }
        
        if (data.size() < headerSize + payloadLength) return;
        
        QByteArray payload = data.mid(headerSize, payloadLength);
        
        if (masked && headerSize >= 6) {
            QByteArray maskKey = data.mid(headerSize - 4, 4);
            for (int i = 0; i < payload.size(); ++i) {
                payload[i] = payload[i] ^ maskKey[i % 4];
            }
        }
        
        if (opcode == 1) { // Text frame
            QString message = QString::fromUtf8(payload);
            emit textMessageReceived(message);
        }
    }

private:
    QTcpSocket *m_socket;
    bool m_handshakeComplete;
};

class CelestronOriginSimulator : public QObject {
    Q_OBJECT
    
public:
    CelestronOriginSimulator(QObject *parent = nullptr) : QObject(parent) {
        // Initialize the dual protocol server
        m_tcpServer = new QTcpServer(this);
        
        if (m_tcpServer->listen(QHostAddress::Any, SERVER_PORT)) {
            qDebug() << "Origin simulator listening on port" << SERVER_PORT;
            qDebug() << "WebSocket: ws://localhost/SmartScope-1.0/mountControlEndpoint";
            qDebug() << "HTTP Images: http://localhost/SmartScope-1.0/dev2/Images/Temp/";
            
            connect(m_tcpServer, &QTcpServer::newConnection, this, &CelestronOriginSimulator::handleNewConnection);
            
            // Create broadcast timer
            broadcastTimer = new QTimer(this);
            connect(broadcastTimer, &QTimer::timeout, this, &CelestronOriginSimulator::sendBroadcast);
            broadcastTimer->start(BROADCAST_INTERVAL);
            
            // Create update timer for regular status updates
            updateTimer = new QTimer(this);
            connect(updateTimer, &QTimer::timeout, this, &CelestronOriginSimulator::sendStatusUpdates);
            updateTimer->start(1000);
            
            // Create slew timer
            slewTimer = new QTimer(this);
            connect(slewTimer, &QTimer::timeout, this, &CelestronOriginSimulator::updateSlew);
            
            // Create imaging timer
            imagingTimer = new QTimer(this);
            connect(imagingTimer, &QTimer::timeout, this, &CelestronOriginSimulator::updateImaging);
            
            // Ensure temp directory exists
            QDir().mkpath("simulator_data/Images/Temp");
            
            // Create dummy images
            createDummyImages();
            
            // Initialize UDP socket for broadcasting
            udpSocket = new QUdpSocket(this);
            
            // First broadcast immediately
            QTimer::singleShot(100, this, &CelestronOriginSimulator::sendBroadcast);
        } else {
            qDebug() << "Failed to start Origin simulator:" << m_tcpServer->errorString();
        }
    }
    
    ~CelestronOriginSimulator() {
        if (m_tcpServer) {
            m_tcpServer->close();
        }
        qDeleteAll(webSocketClients);
    }
    
private slots:
    void handleNewConnection() {
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
    
    void handleIncomingData(QTcpSocket *socket) {
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
    
    void handleWebSocketUpgrade(QTcpSocket *socket, const QByteArray &requestData) {
        WebSocketConnection *wsConn = new WebSocketConnection(socket, this);
        
        if (wsConn->performHandshake(requestData)) {
            webSocketClients.append(wsConn);
            
            connect(wsConn, &WebSocketConnection::textMessageReceived, 
                    this, &CelestronOriginSimulator::processWebSocketCommand);
            connect(wsConn, &WebSocketConnection::disconnected, 
                    this, [this, wsConn]() {
                webSocketClients.removeAll(wsConn);
                wsConn->deleteLater();
            });
            
            qDebug() << "WebSocket connection established for telescope control";
            
            // Send initial status updates
            QTimer::singleShot(500, this, [this, wsConn]() {
                sendMountStatus(wsConn);
                sendFocuserStatus(wsConn);
                sendCameraParams(wsConn);
                sendDiskStatus(wsConn);
                sendTaskControllerStatus(wsConn);
                sendEnvironmentStatus(wsConn);
                sendDewHeaterStatus(wsConn);
                sendOrientationStatus(wsConn);
            });
        } else {
            sendHttpResponse(socket, 400, "text/plain", "Bad WebSocket Request");
        }
    }
    
    void handleHttpImageRequest(QTcpSocket *socket, const QString &path) {
        // Extract filename: /SmartScope-1.0/dev2/Images/Temp/0.jpg -> 0.jpg
        QString fileName = path.split("/").last();
        QString fullPath = QString("simulator_data/Images/Temp/%1").arg(fileName);
        
        QFile imageFile(fullPath);
        if (!imageFile.exists() || !imageFile.open(QIODevice::ReadOnly)) {
            qDebug() << "Image not found:" << fullPath;
            sendHttpResponse(socket, 404, "text/plain", "Image not found");
            return;
        }
        
        QByteArray imageData = imageFile.readAll();
        imageFile.close();
        
        // Determine content type
        QString contentType = "image/jpeg";
        if (fileName.endsWith(".png", Qt::CaseInsensitive)) {
            contentType = "image/png";
        } else if (fileName.endsWith(".tiff", Qt::CaseInsensitive)) {
            contentType = "image/tiff";
        }
        
        sendHttpResponse(socket, 200, contentType, imageData);
        qDebug() << "Served Origin image:" << fileName << "(" << imageData.size() << "bytes)";
    }
    
    void handleHttpAstroImageRequest(QTcpSocket *socket, const QString &path) {
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
    
    void sendHttpResponse(QTcpSocket *socket, int statusCode, 
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
    
    void processWebSocketCommand(const QString &message) {
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
        int sequenceId = obj["SequenceID"].toInt();
        QString source = obj["Source"].toString();
        QString type = obj["Type"].toString();
        
        qDebug() << "Received WebSocket command:" << command << "to" << destination << "from" << source;
        
        // Process the command (use your existing command processing logic)
        processCommand(obj, wsConn);
    }
    
    void sendBroadcast() {
        // Prepare the broadcast message
        QString message = QString("Origin IP Address: %1 Identity: Origin140020 Version: 1.1.4248");
        
        // Get our IP addresses
        QList<QHostAddress> ipAddresses = QNetworkInterface::allAddresses();
        
        for (const QHostAddress &address : ipAddresses) {
            if (address.protocol() == QAbstractSocket::IPv4Protocol && address != QHostAddress::LocalHost) {
                QString broadcastMessage = message.arg(address.toString());
                
                // Send broadcast on all network interfaces
                udpSocket->writeDatagram(
                    broadcastMessage.toUtf8(),
                    QHostAddress::Broadcast,
                    BROADCAST_PORT
                );
                
                qDebug() << "Sent broadcast:" << broadcastMessage;
            }
        }
    }
    
    void sendStatusUpdates() {
        // Update time
        telescopeState.dateTime = QDateTime::currentDateTime();
        
        // Send regular status updates to all WebSocket clients
        sendMountStatusToAll();
        
        // Only send image update every 3 seconds to avoid flooding
        static int imageCounter = 0;
        if (++imageCounter % 3 == 0) {
            // Simulate a new image every 3 seconds
            telescopeState.sequenceNumber++;
            telescopeState.fileLocation = QString("Images/Temp/%1.jpg").arg(telescopeState.sequenceNumber % 10);
            
            // Slightly adjust RA and Dec to simulate movement
            telescopeState.ra += 0.0001;
            telescopeState.dec += 0.00001;
            
            sendNewImageReadyToAll();
        }
        
        // Send other status updates less frequently
        static int statusCounter = 0;
        if (++statusCounter % 5 == 0) {
            sendFocuserStatusToAll();
            sendCameraParamsToAll();
            
            if (statusCounter % 10 == 0) {
                sendEnvironmentStatusToAll();
                sendDiskStatusToAll();
                sendDewHeaterStatusToAll();
                sendOrientationStatusToAll();
            }
        }
    }
    
    void updateSlew() {
        static int slewProgress = 0;
        
        // Simulate slew progress
        slewProgress += 20;  // 20% progress per 500ms
        
        if (slewProgress >= 100) {
            // Slew complete
            telescopeState.isGotoOver = true;
            telescopeState.isSlewing = false;
            telescopeState.ra = telescopeState.targetRa;
            telescopeState.dec = telescopeState.targetDec;
            
            // Stop the timer
            slewTimer->stop();
            slewProgress = 0;
            
            // Update mount status
            sendMountStatusToAll();
            
            qDebug() << "Slew complete";
        }
    }
    
    void updateImaging() {
        // Decrement imaging time
        telescopeState.imagingTimeLeft--;
        
        // Send a new image notification
        sendNewImageReadyToAll();
        
        if (telescopeState.imagingTimeLeft <= 0) {
            // Imaging complete
            telescopeState.isImaging = false;
            imagingTimer->stop();
            
            qDebug() << "Imaging complete";
        }
    }

private:
    // Helper to send JSON to a specific WebSocket client
    void sendJsonMessage(WebSocketConnection *wsConn, const QJsonObject &obj) {
        if (!wsConn) return;
        QJsonDocument doc(obj);
        QString message = doc.toJson();
        wsConn->sendTextMessage(message);
    }
    
    // Helper to send JSON to all WebSocket clients
    void sendJsonMessageToAll(const QJsonObject &obj) {
        QJsonDocument doc(obj);
        QString message = doc.toJson();
        
        for (WebSocketConnection *wsConn : webSocketClients) {
            wsConn->sendTextMessage(message);
        }
    }
    
    // Status update methods (broadcast to all clients)
    void sendMountStatusToAll() { sendMountStatus(nullptr); }
    void sendFocuserStatusToAll() { sendFocuserStatus(nullptr); }
    void sendCameraParamsToAll() { sendCameraParams(nullptr); }
    void sendNewImageReadyToAll() { sendNewImageReady(nullptr); }
    void sendEnvironmentStatusToAll() { sendEnvironmentStatus(nullptr); }
    void sendDiskStatusToAll() { sendDiskStatus(nullptr); }
    void sendDewHeaterStatusToAll() { sendDewHeaterStatus(nullptr); }
    void sendOrientationStatusToAll() { sendOrientationStatus(nullptr); }
    void sendTaskControllerStatusToAll() { sendTaskControllerStatus(nullptr); }
    
    // Mount status notification
    void sendMountStatus(WebSocketConnection *specificClient = nullptr, int sequenceId = -1, const QString &destination = "All") {
        QJsonObject mountStatus;
        mountStatus["Command"] = "GetStatus";
        mountStatus["Destination"] = destination;
        mountStatus["BatteryLevel"] = telescopeState.batteryLevel;
        mountStatus["BatteryVoltage"] = telescopeState.batteryVoltage;
        mountStatus["ChargerStatus"] = telescopeState.chargerStatus;
        mountStatus["Date"] = telescopeState.dateTime.toString("dd MM yyyy");
        mountStatus["Time"] = telescopeState.dateTime.toString("hh:mm:ss");
        mountStatus["TimeZone"] = telescopeState.timeZone;
        mountStatus["Latitude"] = telescopeState.latitude;
        mountStatus["Longitude"] = telescopeState.longitude;
        mountStatus["IsAligned"] = telescopeState.isAligned;
        mountStatus["IsGotoOver"] = telescopeState.isGotoOver;
        mountStatus["IsTracking"] = telescopeState.isTracking;
        mountStatus["NumAlignRefs"] = telescopeState.numAlignRefs;
        mountStatus["Enc0"] = telescopeState.enc0;
        mountStatus["Enc1"] = telescopeState.enc1;
        mountStatus["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
        
        if (sequenceId != -1) {
            // Response to a specific command
            mountStatus["SequenceID"] = sequenceId;
            mountStatus["Source"] = "Mount";
            mountStatus["Type"] = "Response";
            mountStatus["ErrorCode"] = 0;
            mountStatus["ErrorMessage"] = "";
            
            if (specificClient) {
                sendJsonMessage(specificClient, mountStatus);
            }
        } else {
            // Broadcast notification
            mountStatus["SequenceID"] = 4000 + (QDateTime::currentDateTime().toSecsSinceEpoch() % 1000);
            mountStatus["Source"] = "Mount";
            mountStatus["Type"] = "Notification";
            
            if (specificClient) {
                sendJsonMessage(specificClient, mountStatus);
            } else {
                sendJsonMessageToAll(mountStatus);
            }
        }
    }
    
    // Focuser status notification
    void sendFocuserStatus(WebSocketConnection *specificClient = nullptr, int sequenceId = -1, const QString &destination = "All") {
        QJsonObject focuserStatus;
        focuserStatus["Command"] = "GetStatus";
        focuserStatus["Destination"] = destination;
        focuserStatus["Backlash"] = telescopeState.backlash;
        focuserStatus["CalibrationLowerLimit"] = telescopeState.calibrationLowerLimit;
        focuserStatus["CalibrationUpperLimit"] = telescopeState.calibrationUpperLimit;
        focuserStatus["IsCalibrationComplete"] = telescopeState.isCalibrationComplete;
        focuserStatus["IsMoveToOver"] = telescopeState.isMoveToOver;
        focuserStatus["NeedAutoFocus"] = telescopeState.needAutoFocus;
        focuserStatus["PercentageCalibrationComplete"] = telescopeState.percentageCalibrationComplete;
        focuserStatus["Position"] = telescopeState.position;
        focuserStatus["RequiresCalibration"] = telescopeState.requiresCalibration;
        focuserStatus["Velocity"] = telescopeState.velocity;
        focuserStatus["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
        
        if (sequenceId != -1) {
            // Response to a specific command
            focuserStatus["SequenceID"] = sequenceId;
            focuserStatus["Source"] = "Focuser";
            focuserStatus["Type"] = "Response";
            focuserStatus["ErrorCode"] = 0;
            focuserStatus["ErrorMessage"] = "";
            
            if (specificClient) {
                sendJsonMessage(specificClient, focuserStatus);
            }
        } else {
            // Broadcast notification
            focuserStatus["SequenceID"] = 4000 + (QDateTime::currentDateTime().toSecsSinceEpoch() % 1000);
            focuserStatus["Source"] = "Focuser";
            focuserStatus["Type"] = "Notification";
            
            if (specificClient) {
                sendJsonMessage(specificClient, focuserStatus);
            } else {
                sendJsonMessageToAll(focuserStatus);
            }
        }
    }
    
    // Camera parameters notification
    void sendCameraParams(WebSocketConnection *specificClient = nullptr, int sequenceId = -1, const QString &destination = "All") {
        QJsonObject cameraParams;
        cameraParams["Command"] = "GetCaptureParameters";
        cameraParams["Destination"] = destination;
        cameraParams["Binning"] = telescopeState.binning;
        cameraParams["BitDepth"] = telescopeState.bitDepth;
        cameraParams["ColorBBalance"] = telescopeState.colorBBalance;
        cameraParams["ColorGBalance"] = telescopeState.colorGBalance;
        cameraParams["ColorRBalance"] = telescopeState.colorRBalance;
        cameraParams["Exposure"] = telescopeState.exposure;
        cameraParams["ISO"] = telescopeState.iso;
        cameraParams["Offset"] = telescopeState.offset;
        cameraParams["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
        
        if (sequenceId != -1) {
            // Response to a specific command
            cameraParams["SequenceID"] = sequenceId;
            cameraParams["Source"] = "Camera";
            cameraParams["Type"] = "Response";
            cameraParams["ErrorCode"] = 0;
            cameraParams["ErrorMessage"] = "";
            
            if (specificClient) {
                sendJsonMessage(specificClient, cameraParams);
            }
        } else {
            // Broadcast notification
            cameraParams["SequenceID"] = 4000 + (QDateTime::currentDateTime().toSecsSinceEpoch() % 1000);
            cameraParams["Source"] = "Camera";
            cameraParams["Type"] = "Notification";
            
            if (specificClient) {
                sendJsonMessage(specificClient, cameraParams);
            } else {
                sendJsonMessageToAll(cameraParams);
            }
        }
    }
    
    // New image notification
    void sendNewImageReady(WebSocketConnection *specificClient = nullptr) {
        QJsonObject newImage;
        newImage["Command"] = "NewImageReady";
        newImage["Destination"] = "All";
        newImage["Dec"] = telescopeState.dec;
        newImage["Ra"] = telescopeState.ra;
        newImage["FileLocation"] = telescopeState.fileLocation;
        newImage["FovX"] = telescopeState.fovX;
        newImage["FovY"] = telescopeState.fovY;
        newImage["ImageType"] = telescopeState.imageType;
        newImage["Orientation"] = telescopeState.orientation;
        newImage["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
        newImage["SequenceID"] = 4000 + (QDateTime::currentDateTime().toSecsSinceEpoch() % 1000);
        newImage["Source"] = "ImageServer";
        newImage["Type"] = "Notification";
        
        if (specificClient) {
            sendJsonMessage(specificClient, newImage);
        } else {
            sendJsonMessageToAll(newImage);
        }
    }
    
    // Environment status notification
    void sendEnvironmentStatus(WebSocketConnection *specificClient = nullptr, int sequenceId = -1, const QString &destination = "All") {
        QJsonObject envStatus;
        envStatus["Command"] = "GetStatus";
        envStatus["Destination"] = destination;
        envStatus["AmbientTemperature"] = telescopeState.ambientTemperature;
        envStatus["CameraTemperature"] = telescopeState.cameraTemperature;
        envStatus["CpuFanOn"] = telescopeState.cpuFanOn;
        envStatus["CpuTemperature"] = telescopeState.cpuTemperature;
        envStatus["DewPoint"] = telescopeState.dewPoint;
        envStatus["FrontCellTemperature"] = telescopeState.frontCellTemperature;
        envStatus["Humidity"] = telescopeState.humidity;
        envStatus["OtaFanOn"] = telescopeState.otaFanOn;
        envStatus["Recalibrating"] = telescopeState.recalibrating;
        envStatus["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
        
        if (sequenceId != -1) {
            envStatus["SequenceID"] = sequenceId;
            envStatus["Source"] = "Environment";
            envStatus["Type"] = "Response";
            envStatus["ErrorCode"] = 0;
            envStatus["ErrorMessage"] = "";
            
            if (specificClient) {
                sendJsonMessage(specificClient, envStatus);
            }
        } else {
            envStatus["SequenceID"] = 4000 + (QDateTime::currentDateTime().toSecsSinceEpoch() % 1000);
            envStatus["Source"] = "Environment";
            envStatus["Type"] = "Notification";
            
            if (specificClient) {
                sendJsonMessage(specificClient, envStatus);
            } else {
                sendJsonMessageToAll(envStatus);
            }
        }
    }
    
    // Disk status notification
    void sendDiskStatus(WebSocketConnection *specificClient = nullptr, int sequenceId = -1, const QString &destination = "All") {
        QJsonObject diskStatus;
        diskStatus["Command"] = "GetStatus";
        diskStatus["Destination"] = destination;
        diskStatus["Capacity"] = QString::number(telescopeState.capacity);
        diskStatus["FreeBytes"] = QString::number(telescopeState.freeBytes);
        diskStatus["Level"] = telescopeState.level;
        diskStatus["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
        
        if (sequenceId != -1) {
            diskStatus["SequenceID"] = sequenceId;
            diskStatus["Source"] = "Disk";
            diskStatus["Type"] = "Response";
            diskStatus["ErrorCode"] = 0;
            diskStatus["ErrorMessage"] = "";
            
            if (specificClient) {
                sendJsonMessage(specificClient, diskStatus);
            }
        } else {
            diskStatus["SequenceID"] = 1000 + (QDateTime::currentDateTime().toSecsSinceEpoch() % 100);
            diskStatus["Source"] = "Disk";
            diskStatus["Type"] = "Notification";
            
            if (specificClient) {
                sendJsonMessage(specificClient, diskStatus);
            } else {
                sendJsonMessageToAll(diskStatus);
            }
        }
    }
    
    // Dew Heater status notification
    void sendDewHeaterStatus(WebSocketConnection *specificClient = nullptr, int sequenceId = -1, const QString &destination = "All") {
        QJsonObject dewHeaterStatus;
        dewHeaterStatus["Command"] = "GetStatus";
        dewHeaterStatus["Destination"] = destination;
        dewHeaterStatus["Aggression"] = telescopeState.aggression;
        dewHeaterStatus["HeaterLevel"] = telescopeState.heaterLevel;
        dewHeaterStatus["ManualPowerLevel"] = telescopeState.manualPowerLevel;
        dewHeaterStatus["Mode"] = telescopeState.mode;
        dewHeaterStatus["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
        
        if (sequenceId != -1) {
            dewHeaterStatus["SequenceID"] = sequenceId;
            dewHeaterStatus["Source"] = "DewHeater";
            dewHeaterStatus["Type"] = "Response";
            dewHeaterStatus["ErrorCode"] = 0;
            dewHeaterStatus["ErrorMessage"] = "";
            
            if (specificClient) {
                sendJsonMessage(specificClient, dewHeaterStatus);
            }
        } else {
            dewHeaterStatus["SequenceID"] = 4000 + (QDateTime::currentDateTime().toSecsSinceEpoch() % 1000);
            dewHeaterStatus["Source"] = "DewHeater";
            dewHeaterStatus["Type"] = "Notification";
            
            if (specificClient) {
                sendJsonMessage(specificClient, dewHeaterStatus);
            } else {
                sendJsonMessageToAll(dewHeaterStatus);
            }
        }
    }
    
    // Orientation Sensor status notification
    void sendOrientationStatus(WebSocketConnection *specificClient = nullptr, int sequenceId = -1, const QString &destination = "All") {
        QJsonObject orientationStatus;
        orientationStatus["Command"] = "GetStatus";
        orientationStatus["Destination"] = destination;
        orientationStatus["Altitude"] = telescopeState.altitude;
        orientationStatus["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
        
        if (sequenceId != -1) {
            orientationStatus["SequenceID"] = sequenceId;
            orientationStatus["Source"] = "OrientationSensor";
            orientationStatus["Type"] = "Response";
            orientationStatus["ErrorCode"] = 0;
            orientationStatus["ErrorMessage"] = "";
            
            if (specificClient) {
                sendJsonMessage(specificClient, orientationStatus);
            }
        } else {
            orientationStatus["SequenceID"] = 4000 + (QDateTime::currentDateTime().toSecsSinceEpoch() % 1000);
            orientationStatus["Source"] = "OrientationSensor";
            orientationStatus["Type"] = "Notification";
            
            if (specificClient) {
                sendJsonMessage(specificClient, orientationStatus);
            } else {
                sendJsonMessageToAll(orientationStatus);
            }
        }
    }
    
    // Task Controller status notification
    void sendTaskControllerStatus(WebSocketConnection *specificClient = nullptr, int sequenceId = -1, const QString &destination = "All") {
        QJsonObject taskStatus;
        taskStatus["Command"] = "GetStatus";
        taskStatus["Destination"] = destination;
        taskStatus["IsReady"] = telescopeState.isReady;
        taskStatus["Stage"] = telescopeState.stage;
        taskStatus["State"] = telescopeState.state;
        taskStatus["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
        
        if (sequenceId != -1) {
            taskStatus["SequenceID"] = sequenceId;
            taskStatus["Source"] = "TaskController";
            taskStatus["Type"] = "Response";
            taskStatus["ErrorCode"] = 0;
            taskStatus["ErrorMessage"] = "";
            
            if (specificClient) {
                sendJsonMessage(specificClient, taskStatus);
            }
        } else {
            taskStatus["SequenceID"] = 4000 + (QDateTime::currentDateTime().toSecsSinceEpoch() % 1000);
            taskStatus["Source"] = "TaskController";
            taskStatus["Type"] = "Notification";
            
            if (specificClient) {
                sendJsonMessage(specificClient, taskStatus);
            } else {
                sendJsonMessageToAll(taskStatus);
            }
        }
    }
    
    // Command processing method (your existing logic)
    void processCommand(const QJsonObject &obj, WebSocketConnection *wsConn) {
        QString command = obj["Command"].toString();
        QString destination = obj["Destination"].toString();
        int sequenceId = obj["SequenceID"].toInt();
        QString source = obj["Source"].toString();
        QString type = obj["Type"].toString();
        
        if (command == "GetStatus") {
            if (destination == "System") {
                sendSystemVersion(wsConn, sequenceId, source);
            } else if (destination == "Mount") {
                sendMountStatus(wsConn, sequenceId, source);
            } else if (destination == "Focuser") {
                sendFocuserStatus(wsConn, sequenceId, source);
            } else if (destination == "TaskController") {
                sendTaskControllerStatus(wsConn, sequenceId, source);
            } else if (destination == "DewHeater") {
                sendDewHeaterStatus(wsConn, sequenceId, source);
            } else if (destination == "Environment") {
                sendEnvironmentStatus(wsConn, sequenceId, source);
            } else if (destination == "OrientationSensor") {
                sendOrientationStatus(wsConn, sequenceId, source);
            } else if (destination == "Disk") {
                sendDiskStatus(wsConn, sequenceId, source);
            } else if (destination == "FactoryCalibrationController") {
                sendCalibrationStatus(wsConn, sequenceId, source);
            }
        } else if (command == "GetVersion") {
            sendSystemVersion(wsConn, sequenceId, source);
        } else if (command == "GetCaptureParameters") {
            sendCameraParams(wsConn, sequenceId, source);
        } else if (command == "GetFilter") {
            sendCameraFilter(wsConn, sequenceId, source);
        } else if (command == "GetModel") {
            sendSystemModel(wsConn, sequenceId, source);
        } else if (command == "RunInitialize") {
            handleRunInitialize(obj, wsConn, sequenceId, source, destination);
        } else if (command == "StartAlignment") {
            handleStartAlignment(obj, wsConn, sequenceId, source, destination);
        } else if (command == "AddAlignmentPoint") {
            handleAddAlignmentPoint(obj, wsConn, sequenceId, source, destination);
        } else if (command == "FinishAlignment") {
            handleFinishAlignment(obj, wsConn, sequenceId, source, destination);
        } else if (command == "GotoRaDec") {
            handleGotoRaDec(obj, wsConn, sequenceId, source, destination);
        } else if (command == "AbortAxisMovement") {
            handleAbortAxisMovement(obj, wsConn, sequenceId, source, destination);
        } else if (command == "StartTracking") {
            handleStartTracking(obj, wsConn, sequenceId, source, destination);
        } else if (command == "StopTracking") {
            handleStopTracking(obj, wsConn, sequenceId, source, destination);
        } else if (command == "RunImaging") {
            handleRunImaging(obj, wsConn, sequenceId, source, destination);
        } else if (command == "CancelImaging") {
            handleCancelImaging(obj, wsConn, sequenceId, source, destination);
        } else if (command == "MoveToPosition" && destination == "Focuser") {
            handleMoveToPosition(obj, wsConn, sequenceId, source, destination);
        } else if (command == "GetListOfAvailableDirectories" && destination == "ImageServer") {
            handleGetDirectoryList(obj, wsConn, sequenceId, source, destination);
        } else if (command == "GetDirectoryContents" && destination == "ImageServer") {
            handleGetDirectoryContents(obj, wsConn, sequenceId, source, destination);
        } else {
            // Default response for unimplemented commands
            QJsonObject response;
            response["Command"] = command;
            response["Destination"] = source;
            response["ErrorCode"] = 0;
            response["ErrorMessage"] = "";
            response["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
            response["SequenceID"] = sequenceId;
            response["Source"] = destination;
            response["Type"] = "Response";
            
            sendJsonMessage(wsConn, response);
        }
    }
    
    // Command handlers
    void handleRunInitialize(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
        telescopeState.dateTime = QDateTime::currentDateTime();
        telescopeState.isAligned = false;
        telescopeState.isGotoOver = true;
        telescopeState.isTracking = false;
        
        if (obj.contains("Date")) telescopeState.dateTime.setDate(QDate::fromString(obj["Date"].toString(), "dd MM yyyy"));
        if (obj.contains("Time")) telescopeState.dateTime.setTime(QTime::fromString(obj["Time"].toString(), "hh:mm:ss"));
        if (obj.contains("Latitude")) telescopeState.latitude = obj["Latitude"].toDouble();
        if (obj.contains("Longitude")) telescopeState.longitude = obj["Longitude"].toDouble();
        if (obj.contains("TimeZone")) telescopeState.timeZone = obj["TimeZone"].toString();
        
        QJsonObject response;
        response["Command"] = "RunInitialize";
        response["Destination"] = source;
        response["ErrorCode"] = 0;
        response["ErrorMessage"] = "";
        response["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
        response["SequenceID"] = sequenceId;
        response["Source"] = destination;
        response["Type"] = "Response";
        
        sendJsonMessage(wsConn, response);
        sendTaskControllerStatusToAll();
        sendMountStatusToAll();
    }
    
    void handleStartAlignment(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
        telescopeState.isAligned = false;
        telescopeState.numAlignRefs = 0;
        
        QJsonObject response;
        response["Command"] = "StartAlignment";
        response["Destination"] = source;
        response["ErrorCode"] = 0;
        response["ErrorMessage"] = "";
        response["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
        response["SequenceID"] = sequenceId;
        response["Source"] = destination;
        response["Type"] = "Response";
        
        sendJsonMessage(wsConn, response);
        sendMountStatusToAll();
    }
    
    void handleAddAlignmentPoint(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
        telescopeState.numAlignRefs++;
        
        QJsonObject response;
        response["Command"] = "AddAlignmentPoint";
        response["Destination"] = source;
        response["ErrorCode"] = 0;
        response["ErrorMessage"] = "";
        response["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
        response["SequenceID"] = sequenceId;
        response["Source"] = destination;
        response["Type"] = "Response";
        
        sendJsonMessage(wsConn, response);
        sendMountStatusToAll();
    }
    
    void handleFinishAlignment(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
        if (telescopeState.numAlignRefs >= 1) {
            telescopeState.isAligned = true;
        }
        
        QJsonObject response;
        response["Command"] = "FinishAlignment";
        response["Destination"] = source;
        response["ErrorCode"] = 0;
        response["ErrorMessage"] = "";
        response["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
        response["SequenceID"] = sequenceId;
        response["Source"] = destination;
        response["Type"] = "Response";
        
        sendJsonMessage(wsConn, response);
        sendMountStatusToAll();
    }
    
    void handleGotoRaDec(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
        if (telescopeState.isAligned) {
            telescopeState.isGotoOver = false;
            telescopeState.isSlewing = true;
            telescopeState.targetRa = obj["Ra"].toDouble();
            telescopeState.targetDec = obj["Dec"].toDouble();
            
            slewTimer->start(500);
            
            QJsonObject response;
            response["Command"] = "GotoRaDec";
            response["Destination"] = source;
            response["ErrorCode"] = 0;
            response["ErrorMessage"] = "";
            response["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
            response["SequenceID"] = sequenceId;
            response["Source"] = destination;
            response["Type"] = "Response";
            
            sendJsonMessage(wsConn, response);
            sendMountStatusToAll();
        } else {
            QJsonObject response;
            response["Command"] = "GotoRaDec";
            response["Destination"] = source;
            response["ErrorCode"] = 1;
            response["ErrorMessage"] = "Telescope not aligned";
            response["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
            response["SequenceID"] = sequenceId;
            response["Source"] = destination;
            response["Type"] = "Response";
            
            sendJsonMessage(wsConn, response);
        }
    }
    
    void handleAbortAxisMovement(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
        telescopeState.isGotoOver = true;
        telescopeState.isSlewing = false;
        slewTimer->stop();
        
        QJsonObject response;
        response["Command"] = "AbortAxisMovement";
        response["Destination"] = source;
        response["ErrorCode"] = 0;
        response["ErrorMessage"] = "";
        response["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
        response["SequenceID"] = sequenceId;
        response["Source"] = destination;
        response["Type"] = "Response";
        
        sendJsonMessage(wsConn, response);
        sendMountStatusToAll();
    }
    
    void handleStartTracking(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
        telescopeState.isTracking = true;
        
        QJsonObject response;
        response["Command"] = "StartTracking";
        response["Destination"] = source;
        response["ErrorCode"] = 0;
        response["ErrorMessage"] = "";
        response["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
        response["SequenceID"] = sequenceId;
        response["Source"] = destination;
        response["Type"] = "Response";
        
        sendJsonMessage(wsConn, response);
        sendMountStatusToAll();
    }
    
    void handleStopTracking(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
        telescopeState.isTracking = false;
        
        QJsonObject response;
        response["Command"] = "StopTracking";
        response["Destination"] = source;
        response["ErrorCode"] = 0;
        response["ErrorMessage"] = "";
        response["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
        response["SequenceID"] = sequenceId;
        response["Source"] = destination;
        response["Type"] = "Response";
        
        sendJsonMessage(wsConn, response);
        sendMountStatusToAll();
    }
    
    void handleRunImaging(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
        telescopeState.isImaging = true;
        telescopeState.imagingTimeLeft = 30;
        
        imagingTimer->start(1000);
        
        QJsonObject response;
        response["Command"] = "RunImaging";
        response["Destination"] = source;
        response["ErrorCode"] = 0;
        response["ErrorMessage"] = "";
        response["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
        response["SequenceID"] = sequenceId;
        response["Source"] = destination;
        response["Type"] = "Response";
        
        sendJsonMessage(wsConn, response);
    }
    
    void handleCancelImaging(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
        telescopeState.isImaging = false;
        imagingTimer->stop();
        
        QJsonObject response;
        response["Command"] = "CancelImaging";
        response["Destination"] = source;
        response["ErrorCode"] = 0;
        response["ErrorMessage"] = "";
        response["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
        response["SequenceID"] = sequenceId;
        response["Source"] = destination;
        response["Type"] = "Response";
        
        sendJsonMessage(wsConn, response);
    }
    
    void handleMoveToPosition(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
        int targetPosition = obj["Position"].toInt();
        telescopeState.position = targetPosition;
        
        QJsonObject response;
        response["Command"] = "MoveToPosition";
        response["Destination"] = source;
        response["ErrorCode"] = 0;
        response["ErrorMessage"] = "";
        response["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
        response["SequenceID"] = sequenceId;
        response["Source"] = destination;
        response["Type"] = "Response";
        
        sendJsonMessage(wsConn, response);
        sendFocuserStatusToAll();
    }
    
    void handleGetDirectoryList(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
        QJsonObject response;
        response["Command"] = "GetListOfAvailableDirectories";
        response["Destination"] = source;
        response["ErrorCode"] = 0;
        response["ErrorMessage"] = "";
        response["ExpiredAt"] = 0;
        response["SequenceID"] = sequenceId;
        response["Source"] = destination;
        response["Type"] = "Response";
        
        QJsonArray dirList;
        for (const QString &dir : telescopeState.astrophotographyDirs) {
            dirList.append(dir);
        }
        response["DirectoryList"] = dirList;
        
        sendJsonMessage(wsConn, response);
    }
    
    void handleGetDirectoryContents(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
        QString dir = obj["Directory"].toString();
        
        QJsonObject response;
        response["Command"] = "GetDirectoryContents";
        response["Destination"] = source;
        response["ErrorCode"] = 0;
        response["ErrorMessage"] = "";
        response["ExpiredAt"] = 0;
        response["SequenceID"] = sequenceId;
        response["Source"] = destination;
        response["Type"] = "Response";
        
        QJsonArray fileList;
        fileList.append("frame_1.jpg");
        fileList.append("frame_2.jpg");
        fileList.append("frame_3.jpg");
        fileList.append("FinalStackedMaster.tiff");
        response["FileList"] = fileList;
        
        sendJsonMessage(wsConn, response);
    }
    
    // System response methods
    void sendSystemVersion(WebSocketConnection *wsConn, int sequenceId, const QString &destination) {
        QJsonObject versionResponse;
        versionResponse["Command"] = "GetVersion";
        versionResponse["Destination"] = destination;
        versionResponse["ErrorCode"] = 0;
        versionResponse["ErrorMessage"] = "";
        versionResponse["ExpiredAt"] = 0;
        versionResponse["Number"] = telescopeState.versionNumber;
        versionResponse["SequenceID"] = sequenceId;
        versionResponse["Source"] = "System";
        versionResponse["Type"] = "Response";
        versionResponse["Version"] = telescopeState.versionString;
        
        sendJsonMessage(wsConn, versionResponse);
    }
    
    void sendSystemModel(WebSocketConnection *wsConn, int sequenceId, const QString &destination) {
        QJsonObject modelResponse;
        modelResponse["Command"] = "GetModel";
        modelResponse["Destination"] = destination;
        modelResponse["ErrorCode"] = 0;
        modelResponse["ErrorMessage"] = "";
        modelResponse["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
        modelResponse["SequenceID"] = sequenceId;
        modelResponse["Source"] = "System";
        modelResponse["Type"] = "Response";
        modelResponse["Value"] = "Origin";
        
        QJsonArray devices;
        devices.append("System");
        devices.append("TaskController");
        devices.append("Imaging");
        devices.append("Mount");
        devices.append("Focuser");
        devices.append("Camera");
        devices.append("WiFi");
        devices.append("DewHeater");
        devices.append("Environment");
        devices.append("LedRing");
        devices.append("OrientationSensor");
        devices.append("Debug");
        
        modelResponse["Devices"] = devices;
        
        sendJsonMessage(wsConn, modelResponse);
    }
    
    void sendCameraFilter(WebSocketConnection *wsConn, int sequenceId, const QString &destination) {
        QJsonObject filterResponse;
        filterResponse["Command"] = "GetFilter";
        filterResponse["Destination"] = destination;
        filterResponse["ErrorCode"] = 0;
        filterResponse["ErrorMessage"] = "";
        filterResponse["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
        filterResponse["Filter"] = "Clear";
        filterResponse["SequenceID"] = sequenceId;
        filterResponse["Source"] = "Camera";
        filterResponse["Type"] = "Response";
        
        sendJsonMessage(wsConn, filterResponse);
    }
    
    void sendCalibrationStatus(WebSocketConnection *wsConn, int sequenceId, const QString &destination) {
        QJsonObject calResponse;
        calResponse["Command"] = "GetStatus";
        calResponse["Destination"] = destination;
        calResponse["ErrorCode"] = 0;
        calResponse["ErrorMessage"] = "";
        calResponse["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
        calResponse["SequenceID"] = sequenceId;
        calResponse["Source"] = "FactoryCalibrationController";
        calResponse["Type"] = "Response";
        
        sendJsonMessage(wsConn, calResponse);
        
        // Also send notification
        QJsonObject calNotification;
        calNotification["Command"] = "GetStatus";
        calNotification["Destination"] = "All";
        calNotification["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
        calNotification["IsCalibrated"] = true;
        calNotification["NumTimesCollimated"] = 2;
        calNotification["NumTimesHotSpotCentered"] = 2;
        calNotification["SequenceID"] = 4795;
        calNotification["Source"] = "FactoryCalibrationController";
        calNotification["Type"] = "Notification";
        
        QJsonArray completedPhases;
        completedPhases.append("UPDATE");
        completedPhases.append("HARDWARE_CALIBRATION");
        completedPhases.append("DARK_GENERATION");
        completedPhases.append("FLAT_GENERATION");
        completedPhases.append("FA_TEST");
        completedPhases.append("BATTERY");
        
        calNotification["CompletedPhases"] = completedPhases;
        calNotification["CurrentPhase"] = "IDLE";
        
        sendJsonMessage(wsConn, calNotification);
    }
    
    // Create dummy images for testing
    void createDummyImages() {
        // Create directory
        QDir().mkpath("simulator_data/Images/Temp");
        
        // Create 10 dummy images (0.jpg to 9.jpg)
        for (int i = 0; i < 10; ++i) {
            // Create a simple image with text
            QImage image(800, 600, QImage::Format_RGB32);
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
            for (int j = 0; j < 200; ++j) {
                int x = rand() % image.width();
                int y = rand() % image.height();
                int size = (rand() % 3) + 1;
                painter.drawEllipse(x, y, size, size);
            }
            
            // Save the image
            QString fileName = QString("simulator_data/Images/Temp/%1.jpg").arg(i);
            image.save(fileName);
            
            qDebug() << "Created dummy image:" << fileName;
        }
        
        // Create subdirectory for each astrophotography dir
        for (const QString &dir : telescopeState.astrophotographyDirs) {
            QString dirPath = QString("simulator_data/Images/Astrophotography/%1").arg(dir);
            QDir().mkpath(dirPath);
            
            // Create a stacked master image
            QImage masterImage(1024, 768, QImage::Format_RGB32);
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
            for (int j = 0; j < 1000; ++j) {
                // Create a spiral galaxy pattern
                double angle = (rand() % 360) * M_PI / 180.0;
                double distance = (rand() % 300) + 50;
                double xOffset = masterImage.width() / 2 + cos(angle) * distance;
                double yOffset = masterImage.height() / 2 + sin(angle) * distance;
                
                int x = xOffset + (rand() % 20) - 10;
                int y = yOffset + (rand() % 20) - 10;
                
                // Skip if outside image
                if (x < 0 || x >= masterImage.width() || 
                    y < 0 || y >= masterImage.height()) {
                    continue;
                }
                
                int brightness = rand() % 200 + 55;
                painter.setPen(QColor(brightness, brightness, brightness));
                
                int size = (rand() % 4) + 1;
                painter.drawEllipse(x, y, size, size);
            }
            
            // Save the image
            QString fileName = QString("%1/FinalStackedMaster.tiff").arg(dirPath);
            masterImage.save(fileName);
            
            qDebug() << "Created dummy stacked image:" << fileName;
        }
    }
    
private:
    QTcpServer *m_tcpServer;
    QList<WebSocketConnection*> webSocketClients;
    QMap<QTcpSocket*, QByteArray> m_pendingRequests;
    QTimer *broadcastTimer;
    QTimer *updateTimer;
    QTimer *slewTimer;
    QTimer *imagingTimer;
    QUdpSocket *udpSocket;
    TelescopeState telescopeState;
};

// Main function
int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    CelestronOriginSimulator simulator;
    
    qDebug() << "Celestron Origin Simulator is running...";
    qDebug() << "WebSocket endpoint: ws://localhost/SmartScope-1.0/mountControlEndpoint";
    qDebug() << "HTTP images: http://localhost/SmartScope-1.0/dev2/Images/Temp/";
    qDebug() << "Press Ctrl+C to exit.";
    
    return app.exec();
}

#include "OriginSimulator.moc"
