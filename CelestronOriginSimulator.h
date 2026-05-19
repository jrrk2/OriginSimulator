#ifndef CELESTRONORIGINSIMULATOR_H
#define CELESTRONORIGINSIMULATOR_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QTimer>
#include <QDateTime>
#include <QMap>
#include <QList>

#include "TelescopeState.h"
#include "WebSocketConnection.h"
#include "CommandHandler.h"
#include "StatusSender.h"
#include "GaiaStarFieldRenderer.h"
#include "ProperHipsClient.h"
#include "StellariumDSOOverlay.h"

// Constants
const QString SERVER_NAME = "CelestronOriginSimulator";
const int SERVER_PORT = 80;
const int BROADCAST_PORT = 55555;
const int BROADCAST_INTERVAL = 5000; // milliseconds

#define qrand rand

class CelestronOriginSimulator : public QObject {
    Q_OBJECT
    
public:
    // dsoFilter, if non-empty, restricts the Stellarium DSO overlay to entries
    // whose imageUrl matches the substring (case-insensitive) and re-parks the
    // mount at the matching DSO's center coordinates. Used by the
    // --dso=<name> workaround so the App sees the target without searching.
    // rayleighEnabled controls the airmass-driven sky-brightness simulation
    // (default off — the brightening confuses the App's star detector at
    // moderate altitudes; --rayleigh re-enables it for atmospheric realism).
    // dsoAttenuation multiplies the DSO overlay's per-pixel ADU contribution
    // (1.0 = calibrated default, set via --dso-attenuation=N).
    // bortleClass 1..9 — sky-photon contribution to the pedestal (--bortle=N).
    // astroDir overrides the default Astrophotography directory path used to
    // serve real captured sessions; empty → keep TelescopeState default.
    explicit CelestronOriginSimulator(const QString& dsoFilter = {},
                                      bool rayleighEnabled = false,
                                      double dsoAttenuation = 1.0,
                                      int bortleClass = 5,
                                      const QString& astroDir = {},
                                      QObject *parent = nullptr);
    ~CelestronOriginSimulator();

    // ---------------- GUI surface ----------------
    // Read-only state polling.
    const TelescopeState* state() const     { return m_telescopeState; }
    int  connectedClientCount() const       { return m_webSocketClients.size(); }
    int  dsoCatalogueSize() const           { return m_dsoOverlay ? m_dsoOverlay->catalogueSize() : 0; }
    QString lastImageType() const           { return m_telescopeState ? m_telescopeState->imageType : QString(); }

    // Live mutators picked up on the next render/imaging tick.
    bool   rayleighEnabled() const          { return m_rayleighEnabled; }
    void   setRayleighEnabled(bool b)       { m_rayleighEnabled = b; }
    int    bortleClass() const              { return m_bortleClass; }
    void   setBortleClass(int b)            { m_bortleClass = qBound(1, b, 9); }
    double dsoAttenuation() const           { return m_dsoOverlay ? m_dsoOverlay->attenuation() : 1.0; }
    void   setDsoAttenuation(double a)      { if (m_dsoOverlay) m_dsoOverlay->setAttenuation(a); }

    // Stop in-flight imaging (equivalent to a CancelImaging WS command).
    void   guiHaltImaging()
    {
        if (m_telescopeState) m_telescopeState->isImaging = false;
        if (m_imagingTimer)   m_imagingTimer->stop();
    }

private slots:
    void handleNewConnection();
    void handleIncomingData(QTcpSocket *socket);
    void sendBroadcast();
    void sendStatusUpdates();
    void updateSlew();
    void updateAltAzSlew();
    void onWebSocketDisconnected();
    void processWebSocketCommand(const QString &message);
    void handleWebSocketPing(const QByteArray &payload);
    void handleWebSocketPong(const QByteArray &payload);
    void handleWebSocketTimeout();
    void checkConnectionHealth();
    
private:
    // Core components
    QTcpServer *m_tcpServer;
    QUdpSocket *m_udpSocket;
    TelescopeState *m_telescopeState;
    CommandHandler *m_commandHandler;
    StatusSender *m_statusSender;
    GaiaStarFieldRenderer* m_gaiaRenderer;
    StellariumDSOOverlay*  m_dsoOverlay = nullptr; // null if Stellarium isn't installed

    // Separate caches for LIVE and STACK paths so a stacked-master render
    // (which paints the DSO) doesn't bleed into what PixInsight/the App see
    // as the LIVE preview. handleHttpImageRequest serves the live pair;
    // handleHttpAstroImageRequest serves the stack pair.
    QByteArray m_imageDataLive;     // raw 16-bit RGB TIFF, served for Images/Temp/*.tiff
    QByteArray m_previewJpegLive;   // 8-bit JPEG, served for Images/Temp/*.jpg
    QByteArray m_imageDataStack;    // raw 16-bit RGB TIFF, served for Astrophotography/*.tiff
    QByteArray m_previewJpegStack;  // 8-bit JPEG, served for Astrophotography/*.jpg

    // Origin telescope parameters (real sensor: Sony IMX571, 3056x2048 RGB)
    static constexpr double PIXSCALE_ARCSEC = 1.4777;  // 0.00041047 deg/pixel
    static constexpr int    OUTPUT_WIDTH    = 3056;
    static constexpr int    OUTPUT_HEIGHT   = 2048;

    // WebSocket management
    QList<WebSocketConnection*> m_webSocketClients;
    QMap<QTcpSocket*, QByteArray> m_pendingRequests;
    
    // Timers
    QTimer *m_broadcastTimer;
    QTimer *m_updateTimer;
    QTimer *m_slewTimer;
    QTimer *m_altAzSlewTimer;
    QTimer *m_manualSlewTimer;
    QTimer *m_imagingTimer;
    QTimer *m_connectionHealthTimer;
    QTimer *m_initTimer;

    int m_initUpdateCount = 0;
    bool m_pendingSnapshotNotify = false;
    QString m_pendingSnapshotPath;

    // Mount-error state — accumulated random-walk drift in pointing, applied
    // before each render to simulate a real mount tracking imperfectly.
    QDateTime m_simStart = QDateTime::currentDateTime(); // time origin for periodic error
    double m_walkRA  = 0.0;                              // accumulated drift, deg of RA
    double m_walkDec = 0.0;                              // accumulated drift, deg of Dec

    // Alt-az field rotation reference. Captured on the first render of a
    // session and on every slew completion; subsequent renders rotate the
    // tangent plane by (current parallactic angle - reference).
    double m_referenceParAngleRad = 0.0;
    bool   m_hasParAngleRef       = false;

    // Sky-condition toggles set from command-line flags.
    bool   m_rayleighEnabled = false;
    int    m_bortleClass     = 5;

    int broadcast_id = qrand() % 90 + 10;
  
    // Initialization methods
    void setupInitialization();
    void updateInitialization();
    void completeInitialization();
    void failInitialization();
    void setupGaiaRenderer();
    void renderGaiaImageForPosition(double ra_deg, double dec_deg);
    void onImageReady(const QByteArray& tiffData);
    void rebuildPreviewJpeg(const QByteArray& tiffSrc, QByteArray& jpegDst, const char* label);
    void updateImaging();
    // Writes Light<NNNNN>.fits (Bayer GBRG mono, int16/BZERO=32768) for the
    // current stack-depth tick into the active session directory under
    // astroBaseDir. Samples the just-rendered RGB stack TIFF at each Bayer
    // position. Returns true on success.
    bool writeLightFitsForCurrentFrame();
    // Writes info.json (StackedInfo metadata) into the session directory.
    void writeSessionInfoJson();
  
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

};

#endif // CELESTRONORIGINSIMULATOR_H
