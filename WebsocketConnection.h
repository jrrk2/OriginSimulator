#ifndef WEBSOCKETCONNECTION_H
#define WEBSOCKETCONNECTION_H

#include <QObject>
#include <QTcpSocket>
#include <QTimer>

class WebSocketConnection : public QObject {
    Q_OBJECT
    
public:
    explicit WebSocketConnection(QTcpSocket *socket, QObject *parent = nullptr);
    
    void sendTextMessage(const QString &message);
    void sendPongMessage(const QByteArray &payload);
    void sendPingMessage(const QByteArray &payload = QByteArray(30, 0)); // Origin sends 30-byte pings
    bool performHandshake(const QByteArray &requestData);
    
    // Start automatic ping cycle (telescope initiates pings)
    void startPingCycle(int intervalMs = 5000);
    void stopPingCycle();

signals:
    void textMessageReceived(const QString &message);
    void pingReceived(const QByteArray &payload);
    void pongReceived(const QByteArray &payload); // New: handle pong responses
    void disconnected();
    void pingTimeout(); // New: when client doesn't respond to our ping

private slots:
    void handleData();
    void onPingTimeout();
    void sendAutomaticPing(); // New: send periodic pings like real telescope

private:
    QTcpSocket *m_socket;
    bool m_handshakeComplete;
    QTimer *m_pingTimeoutTimer;  // Timeout for ping responses
    QTimer *m_autoPingTimer;     // Automatic ping sender
    bool m_waitingForPong;       // Track if we're waiting for pong response
    QByteArray m_pendingData;    // NEW: Store pending frame data
    int m_pingCounter = 0;
  
    void sendFrame(quint8 opcode, const QByteArray &payload, bool masked = false);
    void processFrame(const QByteArray &data);
};

#endif // WEBSOCKETCONNECTION_H
