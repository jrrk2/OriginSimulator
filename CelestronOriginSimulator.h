#ifndef CELESTRONORIGINSIMULATOR_H
#define CELESTRONORIGINSIMULATOR_H

#include <QObject>
#include <QImage>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QTimer>
#include <QElapsedTimer>
#include <QMap>
#include <QList>

#include "TelescopeState.h"
#include "WebSocketConnection.h"
#include "CommandHandler.h"
#include "StatusSender.h"
#include "ProperHipsClient.h"  // Changed from RubinHipsClient
#include "EnhancedMosaicCreator.h"

// Constants
const QString SERVER_NAME = "CelestronOriginSimulator";
const int SERVER_PORT = 80;          // ORIGIN_SIM_PORT overrides, so that tests can run beside a real telescope
const int BROADCAST_PORT = 55555;
const int BROADCAST_INTERVAL = 5000; // milliseconds

#define qrand rand

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
    void handleWebSocketPong(const QByteArray &payload);
    void handleWebSocketTimeout();
    void checkConnectionHealth();
    
    // ProperHips integration slots (renamed from Rubin)
    void onHipsImageReady(const QString& filename);
    void onHipsTilesAvailable(const QStringList& filenames);
    void onHipsFetchError(const QString& error_message);
    void onHipsTestingComplete();

private:
    // Core components
    QTcpServer *m_tcpServer;
    QUdpSocket *m_udpSocket;
    TelescopeState *m_telescopeState;
    CommandHandler *m_commandHandler;
    StatusSender *m_statusSender;
    ProperHipsClient* m_hipsClient;  // Changed from m_rubinClient
    QByteArray m_imageData;
    // --survey: FITS snapshots built from the HiPS survey mosaic (QImage, 1.61 arcsec/px, centred on m_surveyRaDeg/m_surveyDecDeg) instead of synthetic stars only
    bool m_surveyFits = false;
    QImage m_surveyMosaic;
    double m_surveyRaDeg = 0, m_surveyDecDeg = 0, m_pendingRaDeg = 0, m_pendingDecDeg = 0;
    int m_surveyWaitTicks = 0;

    // WebSocket management
    QList<WebSocketConnection*> m_webSocketClients;
    QMap<QTcpSocket*, QByteArray> m_pendingRequests;
    
    // Timers
    QTimer *m_broadcastTimer;
    QTimer *m_updateTimer;
    QTimer *m_slewTimer;
    QTimer *m_imagingTimer;
    QTimer *m_connectionHealthTimer;
    QTimer *m_initTimer;
    QTimer *m_mountTimer;               // moves the mount model, 10 times a second
    QElapsedTimer m_mountClock;
    std::vector<std::pair<double, MountModel>> m_exposureSamples;   // (julian date, mount) every 0.1 s through an exposure
    bool m_exposureSampling = false;
    void stepMount();
    void handleSimRequest(QTcpSocket *socket, const QString &path);   // GET /sim/state, /sim/truth, /sim/slip?degrees=x: for tests

    EnhancedMosaicCreator* m_mosaicCreator;
    bool m_mosaicInProgress;

    int m_initUpdateCount = 0;

    int broadcast_id = qrand() % 90 + 10;
  
    // Initialization methods
    void setupInitialization();
    void updateInitialization();
    void completeInitialization();
    void failInitialization(int code, const QString &message);

    // Absolute paths for image serving
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
    void printRuntimeInfo();
    void openSimulatorDirectoryInFinder();
    void cleanupApplicationSupportFiles();
    void setupHipsIntegration();  // Renamed from setupRubinIntegration
    
    // HiPS image management
    void fetchHipsImagesForPosition(const SkyPosition& position);
    QString getBestAvailableSurvey() const;
    void generateCurrentSkyImage();
    void onMosaicComplete(const QImage& mosaic);
    void addTelescopeOverlay(QPainter& painter, const QImage& image);
};

#endif // CELESTRONORIGINSIMULATOR_H
