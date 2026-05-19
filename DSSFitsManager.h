#ifndef DSSFITSMANAGER_H
#define DSSFITSMANAGER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QDir>
#include <QSettings>
#include <fitsio.h>
#include <vector>
#include <cmath>

class GaiaStarFieldRenderer;

struct CachedFitsImage {
    QString cacheKey;
    double center_ra_deg;
    double center_dec_deg;
    double width_arcmin;
    double height_arcmin;
    QString fitsFilePath;       // Single FITS cube file
    QDateTime fetchTime;

    bool isValid() const {
        return QFile::exists(fitsFilePath);
    }

    // Check if we can crop OUTPUT_W x OUTPUT_H centered on (ra,dec) from this tile
    bool containsPosition(double ra_deg, double dec_deg,
                          double output_w_arcmin, double output_h_arcmin) const {
        double deltaRA = (ra_deg - center_ra_deg) * cos(center_dec_deg * M_PI / 180.0);
        double deltaDec = dec_deg - center_dec_deg;
        double maxOffRA  = (width_arcmin  - output_w_arcmin)  / 2.0;
        double maxOffDec = (height_arcmin - output_h_arcmin) / 2.0;
        return (fabs(deltaRA * 60.0) <= maxOffRA) && (fabs(deltaDec * 60.0) <= maxOffDec);
    }
};

class DSSFitsManager : public QObject {
    Q_OBJECT

public:
    explicit DSSFitsManager(QObject *parent = nullptr);

    void fetchImageForPosition(double ra_deg, double dec_deg);

    QString getCacheDir() const { return m_cacheDir; }
    void clearCache();
    qint64 getCacheSize() const;
    QList<CachedFitsImage> getCachedImages() const { return m_cachedImages; }

signals:
    void imageReady(const QByteArray& tiffData);
    void fetchError(const QString& error);
    void cacheHit(const QString& info);
    void cacheMiss(const QString& info);

private slots:
    void onNetworkReply();

private:
    QNetworkAccessManager* m_networkManager;
    GaiaStarFieldRenderer* m_gaiaRenderer;
    QString m_cacheDir;
    QSettings* m_cacheIndex;
    QList<CachedFitsImage> m_cachedImages;

    // Origin telescope parameters (real sensor: Sony IMX571, 3056x2048 RGB)
    static constexpr double PIXSCALE_ARCSEC = 1.4777;  // 0.00041047 deg/pixel
    static constexpr int    FETCH_SIZE_PX   = 3200;    // tile with margin for cache reuse
    static constexpr int    OUTPUT_WIDTH    = 3056;
    static constexpr int    OUTPUT_HEIGHT   = 2048;

    // Pending network request
    struct PendingRequest {
        double ra_deg, dec_deg;
        double cache_ra, cache_dec;
        QString cacheKey;
        QString cachePath;
        bool active = false;
    };
    PendingRequest m_pending;

    // Cache management
    void loadCacheIndex();
    void saveCacheIndex();
    CachedFitsImage* findCachedTileContaining(double ra_deg, double dec_deg);
    void addToCacheIndex(const CachedFitsImage& image);

    // Image processing
    QByteArray processFitsTile(const QString& fitsPath,
                               double target_ra, double target_dec,
                               double tile_center_ra, double tile_center_dec);
    QByteArray write16BitRGBTiff(int w, int h,
                                  const std::vector<float>& rCh,
                                  const std::vector<float>& gCh,
                                  const std::vector<float>& bCh);
};

#endif // DSSFITSMANAGER_H
