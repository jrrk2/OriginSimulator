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
    bool performHandshake(const QByteArray &requestData);

signals:
    void textMessageReceived(const QString &message);
    void pingReceived(const QByteArray &payload);
    void disconnected();

private slots:
    void handleData();
    void onPingTimeout();

private:
    QTcpSocket *m_socket;
    bool m_handshakeComplete;
    QTimer *m_pingTimer;
    
    void sendFrame(quint8 opcode, const QByteArray &payload, bool masked = false);
    void processFrame(const QByteArray &data);
};

#endif // WEBSOCKETCONNECTION_H