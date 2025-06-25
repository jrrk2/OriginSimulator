// RubinHipsClient.h - Version without Q_OBJECT
#ifndef RUBINHIPSCLIENT_H
#define RUBINHIPSCLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QMap>
#include <QMutex>
#include <QDir>
#include <QDateTime>
#include <QList>
#include <QStringList>
#include <QString>
#include <QRandomGenerator>
#include <functional>
#include <memory>

// Forward declarations
class TelescopeState;

// Constants
constexpr double PI = 3.14159265358979323846;
constexpr double DEG_TO_RAD = PI / 180.0;
constexpr double RAD_TO_DEG = 180.0 / PI;

struct TileJob {
    int pix;
    int tileX, tileY; // in 0..5 and 0..3
    QString url;
};

// Sky Coordinates Class
class SkyCoordinates {
public:
    double ra_deg;
    double dec_deg;
    double fov_deg;
    int hips_order;
    bool is_valid;
    QString validation_message;
    
    // Viewport dimensions (matching JavaScript clients)
    int viewport_width;
    int viewport_height;
    
    // Constructors
    SkyCoordinates(double ra, double dec, double fov, int width = 1024, int height = 768);
    
    // Methods
    void validateAndNormalize();
    int calculateOptimalOrder();
    QString toString() const;
    bool isInKnownCoverage() const;
    
private:
    void validateRubinCoverage();
};

// HiPS Tile Class
class HipsTile {
public:
    int order;
    long long healpix_pixel;
    bool is_loaded;
    QByteArray data;
    QString content_type;
    QString error_message;
    QDateTime fetch_time;
    
    // Constructors
    HipsTile(int ord, long long pixel);
    
    // Methods
    QString getFilename(const QString& survey_name) const;
    bool saveToFile(const QString& filepath) const;
    bool isValid() const { return is_loaded && !data.isEmpty(); }
};

// HiPS Utilities Class
class HipsUtils {
public:
    // Core HEALPix algorithms
    static long long radecToHealpixNested(double ra_deg, double dec_deg, int order);
    static QList<long long> calculateViewportTiles(const SkyCoordinates& coords);
    static QList<long long> getNeighborPixels(long long central_pixel, int order, int radius);
    
    // Helper functions
    static long long ringToNested(long long ring_pixel, long long nside);
    static double calculatePixelAngularSize(int order);
    static int calculateOptimalOrderForViewport(double fov_deg, int viewport_width, int viewport_height);
    
    // Coordinate utilities
    static void normalizeCoordinates(double& ra_deg, double& dec_deg);
    static bool isValidCoordinate(double ra_deg, double dec_deg);
};

// Survey information structure
struct SurveyInfo {
    QString name;
    QString base_url;
    QString description;
    bool available;
};

// Add this to track tile positions
struct TileInfo {
    std::shared_ptr<HipsTile> tile;
    int gridX, gridY;
    QString filename;
    bool loaded;
};

class RubinHipsClient : public QObject {
    // REMOVED Q_OBJECT macro to avoid MOC issues
    
public:
    explicit RubinHipsClient(QObject *parent = nullptr);
    ~RubinHipsClient();

    void testKnownWorkingURL();
    void compareURLGeneration(TelescopeState *state);
  
    // Main interface methods
    void fetchTilesForCurrentPointing(TelescopeState* telescopeState);
    void fetchTilesAsync(const SkyCoordinates& coords, const QString& survey_name);
    void cancelAllFetches();
    
    // Configuration methods
    void setImageDirectory(const QString& dir);
    QStringList getAvailableSurveys() const;
    
    // Callback functions (modern C++ style)
    std::function<void(const QString&)> onImageReady;
    std::function<void(const QStringList&)> onTilesAvailable;
    std::function<void(const QString&)> onFetchError;
    std::function<void(int, int)> onTilesFetched;
    std::function<void(int, int)> onFetchProgress;
    
    // Manual slot connections (since we removed Q_OBJECT)
    void handleNetworkReply(QNetworkReply* reply);
    void onFetchTimeout();
    void initializeConnections();

    // New cache management methods
    void setCacheEnabled(bool enabled) { m_cacheEnabled = enabled; }
    void setCacheMaxSize(qint64 maxSizeBytes) { m_cacheMaxSize = maxSizeBytes; }
    void clearCache();
    QString getCacheStats() const;
    
    // Live view integration
    QString generateLiveViewWithRubin(const SkyCoordinates& coords, const QString& surveyName);
    void overlayRubinOnLiveView(const QString& liveViewPath, const QString& rubinMosaicPath, const QString& outputPath);
  
private:
    // Cache system
    struct CachedTile {
        QByteArray data;
        QDateTime cacheTime;
        qint64 accessCount;
        QString contentType;
        
        bool isValid() const { 
            return !data.isEmpty() && cacheTime.secsTo(QDateTime::currentDateTime()) < 3600; // 1 hour cache
        }
    };
    
    bool m_cacheEnabled = true;
    qint64 m_cacheMaxSize = 100 * 1024 * 1024; // 100MB default
    qint64 m_currentCacheSize = 0;
    QMap<QString, CachedTile> m_tileCache; // URL -> cached tile
    mutable QMutex m_cacheMutex;
    
    // Cache management methods
    QString getCacheKey(const QString& url) const;
    bool getTileFromCache(const QString& url, QByteArray& data, QString& contentType);
    void storeTileInCache(const QString& url, const QByteArray& data, const QString& contentType);
    void evictOldestTiles();
    void cleanupExpiredTiles();
    
    // Live view integration
    QString m_currentMosaicPath;
    bool m_enableLiveViewIntegration = true;

    // Core methods
    void initializeSurveys();
    void initializeImageDirectory();
    
    // Tile calculation and processing
    QList<std::shared_ptr<HipsTile>> calculateRequiredTiles(const SkyCoordinates& coords);
    void fetchSingleTile(std::shared_ptr<HipsTile> tile, const QString& survey_name);
    void processFetchedTile(std::shared_ptr<HipsTile> tile, const QString& survey_name);
    void generateSyntheticImage(double ra_deg, double dec_deg);
    QMap<QString, TileInfo> m_tileMap; // Track tiles by their unique ID
  void createLiveViewWithRubin(const QString& rubinMosaicPath, const QString& surveyName);
  void handleTileReplyWithProperties(QNetworkReply* reply, QImage* mosaic, 
				     int* completed, int* successful,
				     int totalJobs, const QString& surveyName, int tileSize);

    // URL and image handling
    QString buildTileUrl(const HipsTile* tile, const QString& survey_name, const QString& format = "webp") const;
    QString generateRealisticImage(const SkyCoordinates& coords, const QString& survey_name);
    void updateTelescopeImages(const QStringList& filenames);
    void compositeLiveViewImage();
    bool attemptTileFetch(TelescopeState *state, const QString& surveyName, 
                         int order, int nside, int tileSize, int width, int height);
    QString buildTileUrlForSurvey(const QString& surveyName, int order, int pix);
    bool startTileDownload(const std::vector<TileJob>& jobs, const QString& surveyName,
                          int tileSize, int width, int height);
    void handleTileReply(QNetworkReply* reply, const TileJob& job, QImage* mosaic, 
                        int* completed, int* successful, int totalJobs, 
                        const QString& surveyName, int tileSize);
    void addSurveyOverlay(QImage& image, const QString& surveyName, 
                         int successful, int total);
    void finalizeMosaic(QImage* mosaic, int *completed, int *successful, int totalJobs, const QString& surveyName, int cacheHits);
    // Member variables
    QNetworkAccessManager* m_networkManager;
    QTimer* m_timeoutTimer;
    
    // Survey configuration
    QMap<QString, SurveyInfo> m_surveys;
    QString m_imageDirectory;
    
    // Active fetch tracking
    QMap<QNetworkReply*, std::shared_ptr<HipsTile>> m_pendingTiles;
    QMap<QNetworkReply*, QString> m_pendingSurveys;
    
    // Statistics
    int m_activeFetches;
    int m_totalFetches;
    int m_completedFetches;
    bool m_fetchInProgress;
    qint64 m_totalBytesDownloaded;
};

#endif // RUBINHIPSCLIENT_H
