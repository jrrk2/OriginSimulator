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
    void sendPingMessage(const QByteArray &payload = QByteArray());
    bool performHandshake(const QByteArray &requestData);
    
    // Start automatic ping cycle (telescope initiates pings)
    void startPingCycle(int intervalMs = 5000);
    void stopPingCycle();
    void initializePingCycle(); // NEW: Proper initialization

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
    int m_pingCounter = 0;
    int m_missedPongCount = 0;
  
    void sendFrame(quint8 opcode, const QByteArray &payload, bool masked = false);
    void processFrame(const QByteArray &data);
    void debugFrame(const QByteArray &data, const QString &direction); // NEW: Debug helper
    void resetPingState();
    void verifyTimerSetup();
};

#endif // WEBSOCKETCONNECTION_H
