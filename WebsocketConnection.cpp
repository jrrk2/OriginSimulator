#include "WebSocketConnection.h"
#include <QCryptographicHash>
#include <QDebug>
#include <QTime>

void WebSocketConnection::sendTextMessage(const QString &message) {
    if (!m_handshakeComplete || !m_socket) return;
    
    QByteArray data = message.toUtf8();
    sendFrame(0x01, data); // Text frame
}

void WebSocketConnection::sendPongMessage(const QByteArray &payload) {
    if (!m_handshakeComplete || !m_socket) return;
    
    sendFrame(0x0A, payload); // Pong frame
    qDebug() << "WebSocket pong sent with payload size:" << payload.size();
}

void WebSocketConnection::sendPingMessage(const QByteArray &payload) {
    if (!m_handshakeComplete || !m_socket) return;
    
    sendFrame(0x09, payload); // Ping frame
    m_waitingForPong = true;
    m_pingTimeoutTimer->start(); // Start timeout timer
    qDebug() << "WebSocket ping sent with payload size:" << payload.size();
}

void WebSocketConnection::startPingCycle(int intervalMs) {
    if (m_autoPingTimer->isActive()) {
        m_autoPingTimer->stop();
    }
    m_autoPingTimer->setInterval(intervalMs);
    m_autoPingTimer->setSingleShot(false); // CRITICAL: Must be repeating!
    
    connect(m_autoPingTimer, &QTimer::timeout, this, &WebSocketConnection::sendAutomaticPing, Qt::UniqueConnection);
    
    m_autoPingTimer->start();
    qDebug() << "Started CONTINUOUS ping cycle every" << intervalMs << "ms";
}

void WebSocketConnection::stopPingCycle() {
    m_autoPingTimer->stop();
    m_pingTimeoutTimer->stop();
    m_waitingForPong = false;
    qDebug() << "Stopped automatic ping cycle";
}

void WebSocketConnection::sendFrame(quint8 opcode, const QByteArray &payload, bool masked) {
    QByteArray frame;
    
    // Frame format: FIN(1) + RSV(3) + Opcode(4) + MASK(1) + Payload Length(7+) + Payload
    frame.append(0x80 | opcode); // FIN=1, Opcode
    
    if (payload.size() < 126) {
        frame.append(payload.size() | (masked ? 0x80 : 0x00));
    } else if (payload.size() < 65536) {
        frame.append(126 | (masked ? 0x80 : 0x00));
        frame.append((payload.size() >> 8) & 0xFF);
        frame.append(payload.size() & 0xFF);
    } else {
        frame.append(127 | (masked ? 0x80 : 0x00));
        for (int i = 7; i >= 0; --i) {
            frame.append((payload.size() >> (i * 8)) & 0xFF);
        }
    }
    
    // Note: Server-to-client frames are not masked (as per WebSocket spec)
    frame.append(payload);
    m_socket->write(frame);
}

// In WebSocketConnection.cpp, make sure your performHandshake method includes this:

bool WebSocketConnection::performHandshake(const QByteArray &requestData) {
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
    
    // CRITICAL: Start ping cycle immediately and ensure it continues
    QTimer::singleShot(1000, this, [this]() {
        if (m_handshakeComplete && m_socket && m_socket->state() == QAbstractSocket::ConnectedState) {
            qDebug() << "Starting continuous ping cycle every 5 seconds";
            startPingCycle(5000); // This should create a repeating timer
            
            // Send first ping immediately
            QTimer::singleShot(100, this, &WebSocketConnection::sendAutomaticPing);
        }
    });
    
    return true;
}

void WebSocketConnection::handleData() {
    if (!m_handshakeComplete) return;
    
    QByteArray data = m_socket->readAll();
    processFrame(data);
}

// In WebSocketConnection::processFrame function
void WebSocketConnection::processFrame(const QByteArray &data) {
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
    
    // FIX: Check if we have a complete frame
    if (data.size() < headerSize + payloadLength) {
        // We don't have a complete frame yet, store it for later
        m_pendingData.append(data);
        return;
    }
    
    // If we have pending data from earlier, process it together
    QByteArray frameData = data;
    if (!m_pendingData.isEmpty()) {
        frameData = m_pendingData + data;
        m_pendingData.clear();
    }
    
    QByteArray payload = frameData.mid(headerSize, payloadLength);
    
    if (masked && headerSize >= 6) {
        QByteArray maskKey = frameData.mid(headerSize - 4, 4);
        for (int i = 0; i < payload.size(); ++i) {
            payload[i] = payload[i] ^ maskKey[i % 4];
        }
    }
    
    switch (opcode) {
        case 0x01: // Text frame
            emit textMessageReceived(QString::fromUtf8(payload));
            break;
        case 0x08: // Close frame
            qDebug() << "WebSocket close frame received";
            // FIX: Don't stop ping cycle and don't disconnect - keep connection alive
            //stopPingCycle();
            //m_socket->disconnectFromHost();
            
            // Send a close frame back as acknowledgment but don't actually close
            sendFrame(0x08, payload);
            break;
        case 0x09: // Ping frame (client sent us a ping)
            qDebug() << "WebSocket ping received from client, sending pong";
            sendPongMessage(payload);
            emit pingReceived(payload);
            break;
        case 0x0A: // Pong frame (client responded to our ping)
            qDebug() << "Pong received at" << QTime::currentTime().toString("hh:mm:ss.zzz");
            
            if (m_pingTimeoutTimer->isActive()) {
                m_pingTimeoutTimer->stop();
            }
            m_waitingForPong = false;
            
            // Reset the missed pong counter since we got a response
            m_missedPongCount = 0;
            
            emit pongReceived(payload);
	    break;
        default:
            qDebug() << "Unknown WebSocket frame opcode:" << opcode;
            break;
    }
    
    // FIX: Process any remaining data (in case multiple frames were received)
    if (frameData.size() > headerSize + payloadLength) {
        QByteArray remainingData = frameData.mid(headerSize + payloadLength);
        processFrame(remainingData);
    }
}
// Enhanced WebSocketConnection to match real telescope ping/pong behavior
// Replace the sendAutomaticPing method with this more realistic version:

// CRITICAL FIX: Update sendAutomaticPing method in WebSocketConnection.cpp
// The issue is that the ping cycle stops after receiving a pong
// Replace your sendAutomaticPing method with this version:

void WebSocketConnection::sendAutomaticPing() {
    // CRITICAL: Don't skip pings - the client expects them every 5 seconds regardless
    // The original code had: if (m_waitingForPong) return; 
    // This is WRONG - we must keep pinging even if no pong was received
    
    if (!m_handshakeComplete || !m_socket || m_socket->state() != QAbstractSocket::ConnectedState) {
        qDebug() << "Cannot send ping - connection not ready";
        return;
    }
    
    // Format the ping payload to match real Origin telescope exactly
    QString heartbeatString = QString("ixwebsocket::heartbeat::5s::%1").arg(m_pingCounter);
    QByteArray pingPayload = heartbeatString.toUtf8();
    
    // Pad to exactly 29 bytes like real telescope
    while (pingPayload.size() < 29) {
        pingPayload.append('\0');
    }
    if (pingPayload.size() > 29) {
        pingPayload = pingPayload.left(29);
    }
    
    qDebug() << "Sending automatic ping:" << heartbeatString << "counter:" << m_pingCounter;
    
    // Send the ping
    sendPingMessage(pingPayload);
    m_pingCounter++;
    
    // CRITICAL: Reset any previous timeout and start a new one
    if (m_pingTimeoutTimer->isActive()) {
        m_pingTimeoutTimer->stop();
    }
    m_waitingForPong = true;
    m_pingTimeoutTimer->start(10000); // 10 second timeout for this specific ping
}

void WebSocketConnection::onPingTimeout() {
    qDebug() << "Ping timeout for ping counter:" << (m_pingCounter - 1);
    
    // Don't disconnect immediately - real telescope is more tolerant
    m_waitingForPong = false;
    
    // Increment missed pong counter
    m_missedPongCount++;
    
    qDebug() << "Missed consecutive pongs:" << m_missedPongCount;
    
    // Only disconnect after missing 2 consecutive pongs
    if (m_missedPongCount >= 2) {
        qDebug() << "Too many missed pongs, disconnecting client";
        
        QByteArray closePayload;
        closePayload.append(char(1011 >> 8));
        closePayload.append(char(1011 & 0xFF));
        closePayload.append("Ping timeout");
        
        sendFrame(0x08, closePayload);
        stopPingCycle();
        
        QTimer::singleShot(1000, this, [this]() {
            if (m_socket && m_socket->state() == QAbstractSocket::ConnectedState) {
                m_socket->disconnectFromHost();
            }
        });
        
        // Reset counter for potential future connections
        m_missedPongCount = 0;
    }
    
    emit pingTimeout();
}

// Also update the constructor to initialize the new member:
WebSocketConnection::WebSocketConnection(QTcpSocket *socket, QObject *parent) 
    : QObject(parent), m_socket(socket), m_handshakeComplete(false), 
      m_waitingForPong(false), m_pingCounter(0), m_missedPongCount(0) {
    
    // Connect socket signals
    connect(m_socket, &QTcpSocket::readyRead, this, &WebSocketConnection::handleData);
    connect(m_socket, &QTcpSocket::disconnected, this, &WebSocketConnection::disconnected);
    
    // Initialize ping timeout timer
    m_pingTimeoutTimer = new QTimer(this);
    m_pingTimeoutTimer->setSingleShot(true);
    m_pingTimeoutTimer->setInterval(8000);
    connect(m_pingTimeoutTimer, &QTimer::timeout, this, &WebSocketConnection::onPingTimeout);
    
    // Initialize auto ping timer  
    m_autoPingTimer = new QTimer(this);
    m_autoPingTimer->setSingleShot(false);
    
    qDebug() << "WebSocketConnection created, timers and counters initialized";
}

// Add a method to reset the connection state when needed:
void WebSocketConnection::resetPingState() {
    m_pingCounter = 0;
    m_missedPongCount = 0;
    m_waitingForPong = false;
    
    if (m_pingTimeoutTimer->isActive()) {
        m_pingTimeoutTimer->stop();
    }
    
    qDebug() << "Ping state reset";
}

// Update the verifyTimerSetup method to include the new member:
void WebSocketConnection::verifyTimerSetup() {
    qDebug() << "=== Timer Verification ===";
    qDebug() << "Auto ping timer exists:" << (m_autoPingTimer != nullptr);
    if (m_autoPingTimer) {
        qDebug() << "Auto ping timer active:" << m_autoPingTimer->isActive();
        qDebug() << "Auto ping timer interval:" << m_autoPingTimer->interval();
        qDebug() << "Auto ping timer single shot:" << m_autoPingTimer->isSingleShot();
    }
    
    qDebug() << "Ping timeout timer exists:" << (m_pingTimeoutTimer != nullptr);
    if (m_pingTimeoutTimer) {
        qDebug() << "Ping timeout timer active:" << m_pingTimeoutTimer->isActive();
        qDebug() << "Ping timeout timer interval:" << m_pingTimeoutTimer->interval();
    }
    
    qDebug() << "Handshake complete:" << m_handshakeComplete;
    qDebug() << "Socket state:" << (m_socket ? m_socket->state() : -1);
    qDebug() << "Ping counter:" << m_pingCounter;
    qDebug() << "Missed pong count:" << m_missedPongCount;
    qDebug() << "Waiting for pong:" << m_waitingForPong;
    qDebug() << "========================";
}
