#ifndef STATUSSENDER_H
#define STATUSSENDER_H

#include <QObject>
#include <QJsonObject>
#include <QList>
#include "TelescopeState.h"
#include "WebSocketConnection.h"

class StatusSender : public QObject {
    Q_OBJECT
    
public:
    explicit StatusSender(TelescopeState *state, QObject *parent = nullptr);
    
    void addWebSocketClient(WebSocketConnection *client);
    void removeWebSocketClient(WebSocketConnection *client);
    
    // Send status to specific client or all clients
    void sendMountStatus(WebSocketConnection *specificClient = nullptr, int sequenceId = -1, const QString &destination = "All");
    void sendFocuserStatus(WebSocketConnection *specificClient = nullptr, int sequenceId = -1, const QString &destination = "All");
    void sendCameraParams(WebSocketConnection *specificClient = nullptr, int sequenceId = -1, const QString &destination = "All");
    void sendNewImageReady(WebSocketConnection *specificClient = nullptr);
    void sendEnvironmentStatus(WebSocketConnection *specificClient = nullptr, int sequenceId = -1, const QString &destination = "All");
    void sendDiskStatus(WebSocketConnection *specificClient = nullptr, int sequenceId = -1, const QString &destination = "All");
    void sendDewHeaterStatus(WebSocketConnection *specificClient = nullptr, int sequenceId = -1, const QString &destination = "All");
    void sendOrientationStatus(WebSocketConnection *specificClient = nullptr, int sequenceId = -1, const QString &destination = "All");
    void sendTaskControllerStatus(WebSocketConnection *specificClient = nullptr, int sequenceId = -1, const QString &destination = "All");
    void sendSystemVersion(WebSocketConnection *wsConn, int sequenceId, const QString &destination);
    void sendSystemModel(WebSocketConnection *wsConn, int sequenceId, const QString &destination);
    void sendCameraFilter(WebSocketConnection *wsConn, int sequenceId, const QString &destination);
    void sendCalibrationStatus(WebSocketConnection *wsConn, int sequenceId, const QString &destination);
    
    // Broadcast methods (send to all clients)
    void sendMountStatusToAll() { sendMountStatus(); }
    void sendFocuserStatusToAll() { sendFocuserStatus(); }
    void sendCameraParamsToAll() { sendCameraParams(); }
    void sendNewImageReadyToAll() { sendNewImageReady(); }
    void sendEnvironmentStatusToAll() { sendEnvironmentStatus(); }
    void sendDiskStatusToAll() { sendDiskStatus(); }
    void sendDewHeaterStatusToAll() { sendDewHeaterStatus(); }
    void sendOrientationStatusToAll() { sendOrientationStatus(); }
    void sendTaskControllerStatusToAll() { sendTaskControllerStatus(); }
    // was private
    void sendJsonMessageToAll(const QJsonObject &obj);

private:
    TelescopeState *m_telescopeState;
    QList<WebSocketConnection*> m_webSocketClients;
    
    // Helper methods
    void sendJsonMessage(WebSocketConnection *wsConn, const QJsonObject &obj);
};

#endif // STATUSSENDER_H
