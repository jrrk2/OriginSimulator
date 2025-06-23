#include "WebSocketConnection.h"
#include <QCryptographicHash>
#include <QDebug>

WebSocketConnection::WebSocketConnection(QTcpSocket *socket, QObject *parent) 
    : QObject(parent), m_socket(socket), m_handshakeComplete(false), m_waitingForPong(false) {
    connect(m_socket, &QTcpSocket::readyRead, this, &WebSocketConnection::handleData);
    connect(m_socket, &QTcpSocket::disconnected, this, &WebSocketConnection::disconnected);
    
    // Set up ping timeout timer (for when we send pings and expect pongs)
    m_pingTimeoutTimer = new QTimer(this);
    m_pingTimeoutTimer->setSingleShot(true);
    m_pingTimeoutTimer->setInterval(10000); // 10 second timeout for pong response
    connect(m_pingTimeoutTimer, &QTimer::timeout, this, &WebSocketConnection::onPingTimeout);
    
    // Set up automatic ping timer (telescope sends pings every 5 seconds)
    m_autoPingTimer = new QTimer(this);
    connect(m_autoPingTimer, &QTimer::timeout, this, &WebSocketConnection::sendAutomaticPing);
}

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
    m_autoPingTimer->start();
    qDebug() << "Started automatic ping cycle every" << intervalMs << "ms";
}

void WebSocketConnection::stopPingCycle() {
    m_autoPingTimer->stop();
    m_pingTimeoutTimer->stop();
    m_waitingForPong = false;
    qDebug() << "Stopped automatic ping cycle";
}

void WebSocketConnection::sendAutomaticPing() {
    // Don't send another ping if we're still waiting for a pong
    if (m_waitingForPong) {
        qDebug() << "Skipping ping - still waiting for pong response";
        return;
    }
    
    // Send ping with 30-byte payload like real Origin telescope
    QByteArray pingPayload(30, 0);
    sendPingMessage(pingPayload);
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
    
    // Start the ping cycle once handshake is complete (like real telescope)
    QTimer::singleShot(1000, this, [this]() {
        startPingCycle(5000); // Start pinging every 5 seconds
    });
    
    return true;
}

void WebSocketConnection::handleData() {
    if (!m_handshakeComplete) return;
    
    QByteArray data = m_socket->readAll();
    processFrame(data);
}

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
    
    if (data.size() < headerSize + payloadLength) return;
    
    QByteArray payload = data.mid(headerSize, payloadLength);
    
    if (masked && headerSize >= 6) {
        QByteArray maskKey = data.mid(headerSize - 4, 4);
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
            stopPingCycle();
            m_socket->disconnectFromHost();
            break;
        case 0x09: // Ping frame (client sent us a ping)
            qDebug() << "WebSocket ping received from client, sending pong";
            sendPongMessage(payload);
            emit pingReceived(payload);
            break;
        case 0x0A: // Pong frame (client responded to our ping)
            qDebug() << "WebSocket pong received from client";
            m_pingTimeoutTimer->stop();
            m_waitingForPong = false;
            emit pongReceived(payload);
            break;
        default:
            qDebug() << "Unknown WebSocket frame opcode:" << opcode;
            break;
    }
}

void WebSocketConnection::onPingTimeout() {
    qDebug() << "WebSocket ping timeout - client didn't respond to ping";
    m_waitingForPong = false;
    
    // Send close frame with ping timeout status
    QByteArray closePayload;
    closePayload.append((1011 >> 8) & 0xFF); // Status code 1011 (Internal Error)
    closePayload.append(1011 & 0xFF);
    closePayload.append("Ping timeout");
    
    sendFrame(0x08, closePayload); // Close frame
    stopPingCycle();
    emit pingTimeout();
    m_socket->disconnectFromHost();
}