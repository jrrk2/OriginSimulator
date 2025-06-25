#ifndef CELESTRONORIGINSIMULATOR_H
#define CELESTRONORIGINSIMULATOR_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QTimer>
#include <QMap>
#include <QList>

#include "TelescopeState.h"
#include "WebSocketConnection.h"
#include "CommandHandler.h"
#include "StatusSender.h"
#include "RubinHipsClient.h"

// Constants
const QString SERVER_NAME = "CelestronOriginSimulator";
const int SERVER_PORT = 80;
const int BROADCAST_PORT = 55555;
const int BROADCAST_INTERVAL = 5000; // milliseconds

class CelestronOriginSimulator : public QObject {
    Q_OBJECT
    
public:
    explicit CelestronOriginSimulator(QObject *parent = nullptr);
    ~CelestronOriginSimulator();

private slots:
    void handleNewConnection();
    void handleIncomingData(QTcpSocket *socket);
    void sendBroadcast();
    void sendStatusUpdates();
    void updateSlew();
    void updateImaging();
    void onWebSocketDisconnected();
    void processWebSocketCommand(const QString &message);
    void handleWebSocketPing(const QByteArray &payload);
    void handleWebSocketPong(const QByteArray &payload);  // NEW
    void handleWebSocketTimeout();                        // NEW
    void checkConnectionHealth();                         // NEW
    // Rubin Observatory integration slots
    void onRubinImageReady(const QString& filename);
    void onRubinTilesAvailable(const QStringList& filenames);
    void onRubinFetchError(const QString& error_message);

private:
    // Core components
    QTcpServer *m_tcpServer;
    QUdpSocket *m_udpSocket;
    TelescopeState *m_telescopeState;
    CommandHandler *m_commandHandler;
    StatusSender *m_statusSender;
    RubinHipsClient* m_rubinClient;

    // WebSocket management
    QList<WebSocketConnection*> m_webSocketClients;
    QMap<QTcpSocket*, QByteArray> m_pendingRequests;
    
    // Timers
    QTimer *m_broadcastTimer;
    QTimer *m_updateTimer;
    QTimer *m_slewTimer;
    QTimer *m_imagingTimer;
    QTimer *m_connectionHealthTimer;
    // Add initialization timer
    QTimer *m_initTimer;
    int m_initUpdateCount = 0;  // Add this to track initialization progress
  
    // Add initialization methods
    void setupInitialization();
    void updateInitialization();
    void completeInitialization();
    void failInitialization();

    // Absolute paths for image serving (NEW)
    QString m_absoluteTempDir;
    QString m_absoluteAstroDir;
    
    // Protocol handlers
    void handleWebSocketUpgrade(QTcpSocket *socket, const QByteArray &requestData);
    void handleHttpImageRequest(QTcpSocket *socket, const QString &path);
    void handleHttpAstroImageRequest(QTcpSocket *socket, const QString &path);
    
    // HTTP response helper
    void sendHttpResponse(QTcpSocket *socket, int statusCode, 
                         const QString &contentType, const QByteArray &data);
    
    // Initialization
    void createDummyImagesOld();
    void setupTimers();
    void setupConnections();
    void printRuntimeInfo();                    // NEW
    void openSimulatorDirectoryInFinder();      // NEW
    void cleanupApplicationSupportFiles();      // NEW
    void setupRubinIntegration();

};

#endif // CELESTRONORIGINSIMULATOR_H
