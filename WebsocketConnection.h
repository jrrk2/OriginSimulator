#ifndef WEBSOCKETCONNECTION_H
#define WEBSOCKETCONNECTION_H

#include <QObject>
#include <QTcpSocket>
#include <QTimer>

class WebSocketConnection : public QObject {
    Q_OBJECT
    
public:
    // Updated constructor with optional delayed ownership
    explicit WebSocketConnection(QTcpSocket *socket, QObject *parent = nullptr, bool takeOwnership = true);
    
    void sendTextMessage(const QString &message);
    void sendPongMessage(const QByteArray &payload);
    void sendPingMessage(const QByteArray &payload = QByteArray());
    bool performHandshake(const QByteArray &requestData);
    
    // New method to take ownership after handshake
    void takeSocketOwnership();
    
    // Start automatic ping cycle (telescope initiates pings)
    void startPingCycle(int intervalMs = 5000);
    void stopPingCycle();
    
    // Debug and monitoring methods
    void resetPingState();
    void verifyTimerSetup();
    void verifySocketOwnership();

signals:
    void textMessageReceived(const QString &message);
    void pingReceived(const QByteArray &payload);
    void pongReceived(const QByteArray &payload);
    void disconnected();
    void pingTimeout();

private slots:
    void handleData();
    void onPingTimeout();
    void sendAutomaticPing();

private:
    QTcpSocket *m_socket;
    bool m_handshakeComplete;
    QTimer *m_pingTimeoutTimer;
    QTimer *m_autoPingTimer;
    bool m_waitingForPong;
    QByteArray m_pendingData;    // For handling incomplete frames
    int m_pingCounter;
    int m_missedPongCount;
  
    void sendFrame(quint8 opcode, const QByteArray &payload, bool masked = false);
    int processFrame(const QByteArray &data);
};

#endif // WEBSOCKETCONNECTION_H