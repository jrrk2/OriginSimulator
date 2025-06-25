#ifndef RUBINHIPSCLIENT_H
#define RUBINHIPSCLIENT_H

#include <QtCore>
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QDir>
#include <QStandardPaths>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMutex>
#include <QRandomGenerator>
#include <QImage>
#include <QPainter>
#include <QRadialGradient>
#include <functional>

// Forward declarations
class TelescopeState;

/**
 * Sky coordinates with validation for Rubin Observatory coverage
 */
struct SkyCoordinates {
    double ra_deg;
    double dec_deg;
    double fov_deg;
    int hips_order;
    bool is_valid;
    QString validation_message;
    
    SkyCoordinates(double ra, double dec, double fov);
    void validateAndNormalize();
    void validateRubinCoverage();
    QString toString() const;
    bool isInKnownCoverage() const;
};

/**
 * HiPS tile data structure
 */
struct HipsTile {
    int order;
    long long healpix_pixel;
    QByteArray data;
    QString content_type;
    bool is_loaded;
    QDateTime fetch_time;
    QString error_message;
    
    HipsTile(int ord, long long pixel);
    QString getFilename(const QString& survey_name) const;
    bool saveToFile(const QString& filepath) const;
};

/**
 * Qt-based Rubin Observatory HiPS client integrated with telescope simulator
 */
class RubinHipsClient : public QObject {
    // Remove Q_OBJECT macro - not needed for this approach
    
public:
    explicit RubinHipsClient(QObject *parent = nullptr);
    ~RubinHipsClient();
    
    // Main interface methods
    void fetchTilesForCurrentPointing(TelescopeState* telescopeState);
    void fetchTilesAsync(const SkyCoordinates& coords, const QString& survey_name = "virgo_cluster");
    QStringList getAvailableSurveys() const;
    
    // Configuration
    void setImageDirectory(const QString& dir);
    QString getImageDirectory() const { return m_imageDirectory; }
    
    // Status
    bool isFetching() const { return m_activeFetches > 0; }
    int getActiveFetches() const { return m_activeFetches; }
    qint64 getTotalBytesDownloaded() const { return m_totalBytesDownloaded; }
    
    // Callback functions - simpler than signals/slots
    std::function<void(int successful_tiles, int total_tiles)> onTilesFetched;
    std::function<void(const QStringList& filenames)> onTilesAvailable;
    std::function<void(int completed, int total)> onFetchProgress;
    std::function<void(const QString& error_message)> onFetchError;
    std::function<void(const QString& filename)> onImageReady;

public: // Methods (no longer slots)
    void cancelAllFetches();

private: // Methods (no longer slots)
    void compositeLiveViewImage();
    void handleNetworkReply();
    void onFetchTimeout();
    // Network management
    QNetworkAccessManager* m_networkManager;
    QTimer* m_timeoutTimer;
    QMutex m_fetchMutex;
    
    // State tracking
    int m_activeFetches;
    int m_totalFetches;
    int m_completedFetches;
    qint64 m_totalBytesDownloaded;
    QString m_imageDirectory;
    
    // Survey configuration
    struct HipsSurvey {
        QString name;
        QString base_url;
        QString description;
        bool available;
    };
    QMap<QString, HipsSurvey> m_surveys;
    
public:
    // HiPS utilities - moved to public section
    class HipsUtils {
    public:
        static long long radecToHealpixNested(double ra_deg, double dec_deg, int tile_order);
        static int calculateOrder(double fov_deg);
        static QList<long long> getKnownWorkingPixels(int order);
        static QList<long long> calculateNeighborPixels(long long central_pixel, int order, int radius = 2);
    };
    
    // Private methods
    void initializeSurveys();
    void initializeImageDirectory();
    QList<std::shared_ptr<HipsTile>> calculateRequiredTiles(const SkyCoordinates& coords);
    void fetchSingleTile(std::shared_ptr<HipsTile> tile, const QString& survey_name);
    QString buildTileUrl(const HipsTile* tile, const QString& survey_name, const QString& format = "webp") const;
    void processFetchedTile(std::shared_ptr<HipsTile> tile, const QString& survey_name);
    QString generateRealisticImage(const SkyCoordinates& coords, const QString& survey_name);
    void updateTelescopeImages(const QStringList& filenames);
    
    // Pending network operations
    QMap<QNetworkReply*, std::shared_ptr<HipsTile>> m_pendingTiles;
    QMap<QNetworkReply*, QString> m_pendingSurveys;
};

#endif // RUBINHIPSCLIENT_H
