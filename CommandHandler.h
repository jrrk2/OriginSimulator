#ifndef COMMANDHANDLER_H
#define COMMANDHANDLER_H

#include <QObject>
#include <QJsonObject>
#include "TelescopeState.h"
#include "WebSocketConnection.h"

class CommandHandler : public QObject {
    Q_OBJECT
    
public:
    explicit CommandHandler(TelescopeState *state, QObject *parent = nullptr);
    
    void processCommand(const QJsonObject &obj, WebSocketConnection *wsConn);

signals:
    void slewStarted();
    void imagingStarted();

private:
    TelescopeState *m_telescopeState;
    
    // Command handlers
    void handleRunInitialize(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination);
    void handleStartAlignment(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination);
    void handleAddAlignmentPoint(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination);
    void handleFinishAlignment(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination);
    void handleGotoRaDec(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination);
    void handleAbortAxisMovement(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination);
    void handleStartTracking(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination);
    void handleStopTracking(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination);
    void handleRunImaging(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination);
    void handleCancelImaging(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination);
    void handleMoveToPosition(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination);
    void handleGetDirectoryList(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination);
    void handleGetDirectoryContents(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination);
    void handleSetCaptureParameters(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination);
    void handleSetFocuserBacklash(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination);
    void handleSetDewHeaterMode(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination);
    
    // Helper method to send JSON responses
    void sendJsonResponse(WebSocketConnection *wsConn, const QJsonObject &response);
};

#endif // COMMANDHANDLER_H