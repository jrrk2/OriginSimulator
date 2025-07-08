// CRITICAL DEBUG VERSION - WebSocketConnection.cpp with extensive logging
// This will help us see exactly what's happening with frame processing

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
    
//     // if (false) qDebug() << "*** SENDING PONG MESSAGE ***";
//     if (false) qDebug() << "Payload size:" << payload.size();
//     if (false) qDebug() << "Payload content:" << payload.toHex();
    
    sendFrame(0x0A, payload); // Pong frame
    
//     // if (false) qDebug() << "*** PONG SENT SUCCESSFULLY ***";
}

void WebSocketConnection::sendPingMessage(const QByteArray &payload) {
    if (!m_handshakeComplete || !m_socket) return;
    
    sendFrame(0x09, payload); // Ping frame
    m_waitingForPong = true;
    m_pingTimeoutTimer->start(); // Start timeout timer
//     if (false) qDebug() << "WebSocket ping sent with payload size:" << payload.size();
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
    
    if (0) if (false) qDebug() << "Sending WebSocket frame - Opcode:" << QString("0x%1").arg(opcode, 2, 16, QChar('0'))
             << "Size:" << frame.size() << "Payload size:" << payload.size();
    
    qint64 bytesWritten = m_socket->write(frame);
//     if (false) qDebug() << "Bytes written to socket:" << bytesWritten << "Expected:" << frame.size();
    
    // Force flush to ensure data is sent immediately
    m_socket->flush();
}


// Updated performHandshake to set completion flag and start ping cycle
bool WebSocketConnection::performHandshake(const QByteArray &requestData) {
    QString request = QString::fromUtf8(requestData);
    QStringList lines = request.split("\r\n");
    
//     // if (false) qDebug() << "*** PERFORMING WEBSOCKET HANDSHAKE ***";
//     if (false) qDebug() << "Request lines:" << lines.size();
    
    QString webSocketKey;
    for (const QString &line : lines) {
        if (line.startsWith("Sec-WebSocket-Key:", Qt::CaseInsensitive)) {
            webSocketKey = line.mid(18).trimmed();
            break;
        }
    }
    
    if (webSocketKey.isEmpty()) {
//         // if (false) qDebug() << "*** HANDSHAKE FAILED: No WebSocket key found ***";
        return false;
    }
    
//     if (false) qDebug() << "WebSocket key found:" << webSocketKey;
    
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
    
    qint64 bytesWritten = m_socket->write(response.toUtf8());
    m_socket->flush();
    
//     // if (false) qDebug() << "*** HANDSHAKE RESPONSE SENT ***";
//     if (false) qDebug() << "Response bytes written:" << bytesWritten;
    
    m_handshakeComplete = true;
    
    // Start ping cycle after handshake
    QTimer::singleShot(1000, this, [this]() {
        if (m_handshakeComplete && m_socket && m_socket->state() == QAbstractSocket::ConnectedState) {
//             if (false) qDebug() << "Starting continuous ping cycle every 5 seconds";
            startPingCycle(5000);
            
            // Send first ping after a short delay
            QTimer::singleShot(100, this, &WebSocketConnection::sendAutomaticPing);
        }
    });
    
    return true;
}

// Enhanced data handling with better debugging
void WebSocketConnection::handleData() {
    if (!m_handshakeComplete) {
//         // if (false) qDebug() << "*** DATA RECEIVED BEFORE HANDSHAKE COMPLETE ***";
        return;
    }
    
    qint64 bytesAvailable = m_socket->bytesAvailable();
//     // if (false) qDebug() << "*** WEBSOCKET DATA HANDLER CALLED ***";
//     if (false) qDebug() << "Bytes available:" << bytesAvailable;
//     if (false) qDebug() << "Socket state:" << m_socket->state();
    
    if (bytesAvailable == 0) {
//         if (false) qDebug() << "WARNING: readyRead fired but no bytes available";
        return;
    }
    
    // Read all available data
    QByteArray newData = m_socket->read(bytesAvailable);
//     if (false) qDebug() << "Successfully read:" << newData.size() << "bytes";
//     if (false) qDebug() << "Data hex (first 64 bytes):" << newData.left(64).toHex();
    
    if (newData.isEmpty()) {
//         if (false) qDebug() << "ERROR: Read returned empty despite bytesAvailable =" << bytesAvailable;
        return;
    }
    
    m_pendingData.append(newData);
//     if (false) qDebug() << "Total pending data size:" << m_pendingData.size();
    
    // Process all complete frames
    while (!m_pendingData.isEmpty()) {
        int frameSize = processFrame(m_pendingData);
        if (frameSize <= 0) {
//             if (false) qDebug() << "Waiting for more data to complete frame";
            break;
        }
//         if (false) qDebug() << "Processed frame of" << frameSize << "bytes";
        m_pendingData.remove(0, frameSize);
//         if (false) qDebug() << "Remaining pending data:" << m_pendingData.size();
    }
}

// HEAVILY INSTRUMENTED VERSION for debugging
int WebSocketConnection::processFrame(const QByteArray &data) {
//     if (false) qDebug() << "\n*** PROCESSING FRAME ***";
//     if (false) qDebug() << "Input data size:" << data.size();
    
    if (data.size() < 2) {
//         if (false) qDebug() << "Not enough data for frame header";
        return 0; // Not enough data for a frame header
    }
    
    quint8 firstByte = data[0];
    quint8 secondByte = data[1];
    
    bool fin = (firstByte & 0x80) != 0;
    quint8 opcode = firstByte & 0x0F;
    bool masked = (secondByte & 0x80) != 0;
    quint64 payloadLength = secondByte & 0x7F;
    
    if (0) if (false) qDebug() << "Frame header - FIN:" << fin << "Opcode:" << QString("0x%1").arg(opcode, 2, 16, QChar('0')) 
             << "Masked:" << masked << "Initial payload length:" << payloadLength;
    
    int headerSize = 2;
    
    if (payloadLength == 126) {
        if (data.size() < 4) {
//             if (false) qDebug() << "Need extended length but not enough data";
            return 0;
        }
        payloadLength = (quint8(data[2]) << 8) | quint8(data[3]);
        headerSize = 4;
//         if (false) qDebug() << "Extended length (16-bit):" << payloadLength;
    } else if (payloadLength == 127) {
        if (data.size() < 10) {
//             if (false) qDebug() << "Need 64-bit length but not enough data";
            return 0;
        }
        payloadLength = 0;
        for (int i = 0; i < 8; ++i) {
            payloadLength = (payloadLength << 8) | quint8(data[2 + i]);
        }
        headerSize = 10;
//         if (false) qDebug() << "Extended length (64-bit):" << payloadLength;
    }
    
    if (masked) {
        headerSize += 4; // Mask key
//         if (false) qDebug() << "Frame is masked, header size:" << headerSize;
    }
    
    // Check if we have a complete frame
    qint64 totalFrameSize = headerSize + payloadLength;
//     if (false) qDebug() << "Total frame size needed:" << totalFrameSize << "Available:" << data.size();
    
    if (data.size() < totalFrameSize) {
//         if (false) qDebug() << "Incomplete frame - need" << totalFrameSize << "have" << data.size();
        return 0; // Incomplete frame
    }
    
    QByteArray payload = data.mid(headerSize, payloadLength);
//     if (false) qDebug() << "Extracted payload size:" << payload.size();
    
    if (masked && headerSize >= 6) {
        QByteArray maskKey = data.mid(headerSize - 4, 4);
//         if (false) qDebug() << "Unmasking with key:" << maskKey.toHex();
        for (int i = 0; i < payload.size(); ++i) {
            payload[i] = payload[i] ^ maskKey[i % 4];
        }
//         if (false) qDebug() << "Unmasked payload:" << payload.left(32).toHex(); // First 32 bytes
    }
    
    // Process the frame based on opcode
//     if (false) qDebug() << "Processing opcode:" << QString("0x%1").arg(opcode, 2, 16, QChar('0'));
    
    switch (opcode) {
        case 0x01: // Text frame
//             if (false) qDebug() << "TEXT FRAME received:" << QString::fromUtf8(payload).left(100);
            emit textMessageReceived(QString::fromUtf8(payload));
            break;
            
        case 0x08: // Close frame
//             if (false) qDebug() << "CLOSE FRAME received";
            sendFrame(0x08, payload);
            stopPingCycle();
            QTimer::singleShot(1000, this, [this]() {
                if (m_socket && m_socket->state() == QAbstractSocket::ConnectedState) {
                    m_socket->disconnectFromHost();
                }
            });
            break;
            
        case 0x09: // Ping frame (client sent us a ping - CRITICAL!)
//             // if (false) qDebug() << "*** PING FRAME RECEIVED FROM CLIENT ***";
//             if (false) qDebug() << "Payload size:" << payload.size();
//             if (false) qDebug() << "Payload content:" << payload;
//             if (false) qDebug() << "Payload hex:" << payload.toHex();
            
            // IMMEDIATELY send pong response
//             if (false) qDebug() << "Sending PONG response immediately...";
            sendPongMessage(payload);
            
            emit pingReceived(payload);
//             // if (false) qDebug() << "*** PING PROCESSING COMPLETE ***";
            break;
            
        case 0x0A: // Pong frame (client responded to our ping)
//             if (false) qDebug() << "PONG FRAME received from client at" << QTime::currentTime().toString("hh:mm:ss.zzz");
//             if (false) qDebug() << "Pong payload:" << payload;
            
            if (m_pingTimeoutTimer->isActive()) {
                m_pingTimeoutTimer->stop();
            }
            m_waitingForPong = false;
            m_missedPongCount = 0; // Reset counter
            
            emit pongReceived(payload);
            break;
            
        default:
//             if (false) qDebug() << "Unknown WebSocket frame opcode:" << QString("0x%1").arg(opcode, 2, 16, QChar('0'));
            break;
    }
    
//     // if (false) qDebug() << "*** FRAME PROCESSING COMPLETE - returning" << totalFrameSize << "***\n";
    
    // Return the size of the processed frame
    return totalFrameSize;
}

void WebSocketConnection::startPingCycle(int intervalMs) {
    if (m_autoPingTimer->isActive()) {
        m_autoPingTimer->stop();
    }
    m_autoPingTimer->setInterval(intervalMs);
    m_autoPingTimer->setSingleShot(false); // CRITICAL: Must be repeating!
    
    connect(m_autoPingTimer, &QTimer::timeout, this, &WebSocketConnection::sendAutomaticPing, Qt::UniqueConnection);
    
    m_autoPingTimer->start();
//     if (false) qDebug() << "Started CONTINUOUS ping cycle every" << intervalMs << "ms";
}

void WebSocketConnection::stopPingCycle() {
    m_autoPingTimer->stop();
    m_pingTimeoutTimer->stop();
    m_waitingForPong = false;
//     if (false) qDebug() << "Stopped automatic ping cycle";
}

void WebSocketConnection::sendAutomaticPing() {
    if (!m_handshakeComplete || !m_socket || m_socket->state() != QAbstractSocket::ConnectedState) {
//         if (false) qDebug() << "Cannot send ping - connection not ready";
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
    
//     if (false) qDebug() << "Sending automatic ping:" << heartbeatString << "counter:" << m_pingCounter;
    
    // Send the ping
    sendPingMessage(pingPayload);
    m_pingCounter++;
    
    // Start timeout for this specific ping
    if (m_pingTimeoutTimer->isActive()) {
        m_pingTimeoutTimer->stop();
    }
    m_waitingForPong = true;
    m_pingTimeoutTimer->start(15000); // 15 second timeout
}

void WebSocketConnection::onPingTimeout() {
//     if (false) qDebug() << "Ping timeout for ping counter:" << (m_pingCounter - 1);
    
    m_waitingForPong = false;
    m_missedPongCount++;
    
//     if (false) qDebug() << "Missed consecutive pongs:" << m_missedPongCount;
    
    // Only disconnect after missing 3 consecutive pongs (more tolerant)
    if (m_missedPongCount >= 3) {
//         if (false) qDebug() << "Too many missed pongs, disconnecting client";
        
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
        
        m_missedPongCount = 0;
    }
    
    emit pingTimeout();
}

// Updated WebSocketConnection constructor with delayed ownership option
WebSocketConnection::WebSocketConnection(QTcpSocket *socket, QObject *parent, bool takeOwnership) 
    : QObject(parent), m_socket(socket), m_handshakeComplete(false), 
      m_waitingForPong(false), m_pingCounter(0), m_missedPongCount(0) {
    
//     // if (false) qDebug() << "*** WebSocketConnection created ***";
//     if (false) qDebug() << "Take immediate ownership:" << takeOwnership;
//     if (false) qDebug() << "Socket state:" << m_socket->state();
    
    // Initialize timers
    m_pingTimeoutTimer = new QTimer(this);
    m_pingTimeoutTimer->setSingleShot(true);
    m_pingTimeoutTimer->setInterval(15000);
    connect(m_pingTimeoutTimer, &QTimer::timeout, this, &WebSocketConnection::onPingTimeout);
    
    m_autoPingTimer = new QTimer(this);
    m_autoPingTimer->setSingleShot(false);
    
    // Only take ownership immediately if requested (for backward compatibility)
    if (takeOwnership) {
        takeSocketOwnership();
    }
}

// New method to take socket ownership after handshake
void WebSocketConnection::takeSocketOwnership() {
//     // if (false) qDebug() << "*** TAKING EXCLUSIVE SOCKET OWNERSHIP ***";
//     if (false) qDebug() << "Bytes available:" << m_socket->bytesAvailable();
    
    // CRITICAL: Use DirectConnection for immediate processing
    connect(m_socket, &QTcpSocket::readyRead, 
            this, &WebSocketConnection::handleData, 
            Qt::DirectConnection);
    
    connect(m_socket, &QTcpSocket::disconnected, 
            this, &WebSocketConnection::disconnected);
    
//     // if (false) qDebug() << "*** SOCKET OWNERSHIP ESTABLISHED ***";
}

void WebSocketConnection::resetPingState() {
    m_pingCounter = 0;
    m_missedPongCount = 0;
    m_waitingForPong = false;
    
    if (m_pingTimeoutTimer->isActive()) {
        m_pingTimeoutTimer->stop();
    }
    
//     if (false) qDebug() << "Ping state reset";
}

void WebSocketConnection::verifyTimerSetup() {
//     if (false) qDebug() << "=== COMPREHENSIVE Timer Verification ===";
//     if (false) qDebug() << "Auto ping timer exists:" << (m_autoPingTimer != nullptr);
    if (m_autoPingTimer) {
//         if (false) qDebug() << "Auto ping timer active:" << m_autoPingTimer->isActive();
//         if (false) qDebug() << "Auto ping timer interval:" << m_autoPingTimer->interval();
//         if (false) qDebug() << "Auto ping timer single shot:" << m_autoPingTimer->isSingleShot();
    }
    
//     if (false) qDebug() << "Ping timeout timer exists:" << (m_pingTimeoutTimer != nullptr);
    if (m_pingTimeoutTimer) {
//         if (false) qDebug() << "Ping timeout timer active:" << m_pingTimeoutTimer->isActive();
//         if (false) qDebug() << "Ping timeout timer interval:" << m_pingTimeoutTimer->interval();
    }
    
//     if (false) qDebug() << "Handshake complete:" << m_handshakeComplete;
//     if (false) qDebug() << "Socket state:" << (m_socket ? m_socket->state() : -1);
//     if (false) qDebug() << "Ping counter:" << m_pingCounter;
//     if (false) qDebug() << "Missed pong count:" << m_missedPongCount;
//     if (false) qDebug() << "Waiting for pong:" << m_waitingForPong;
//     if (false) qDebug() << "Pending data size:" << m_pendingData.size();
//     if (false) qDebug() << "=======================================";
}
