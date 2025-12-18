#ifndef DSSFITSMANAGER_H
#define DSSFITSMANAGER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QImage>
#include <QDir>
#include <QSettings>
#include <fitsio.h>
#include "ProperHipsClient.h"

enum class DSSurvey {
    POSS2UKSTU_RED,
    POSS2UKSTU_BLUE,
    POSS2UKSTU_IR,
    POSS1_RED,
    POSS1_BLUE,
    QUICKV
};

struct CachedFitsImage {
    QString cacheKey;           // Unique identifier
    double center_ra_deg;       // Center coordinates
    double center_dec_deg;
    double width_arcmin;        // Field size
    double height_arcmin;
    QString irFilePath;         // Paths to cached FITS files
    QString redFilePath;
    QString blueFilePath;
    QDateTime fetchTime;
    
    bool isValid() const {
        return QFile::exists(irFilePath) && 
               QFile::exists(redFilePath) && 
               QFile::exists(blueFilePath);
    }
    
    bool containsPosition(double ra_deg, double dec_deg, double margin_arcmin = 5.0) const {
        // Check if position is within the cached area (with margin)
        double effectiveWidth = width_arcmin - margin_arcmin;
        double effectiveHeight = height_arcmin - margin_arcmin;
        
        double deltaRA = (ra_deg - center_ra_deg) * cos(center_dec_deg * M_PI / 180.0);
        double deltaDec = dec_deg - center_dec_deg;
        
        double deltaRA_arcmin = fabs(deltaRA * 60.0);
        double deltaDec_arcmin = fabs(deltaDec * 60.0);
        
        return (deltaRA_arcmin <= effectiveWidth / 2.0) && 
               (deltaDec_arcmin <= effectiveHeight / 2.0);
    }
};

class DSSFitsManager : public QObject {
    Q_OBJECT

public:
    explicit DSSFitsManager(QObject *parent = nullptr);
    
    // Main interface
    void fetchImageForPosition(double ra_deg, double dec_deg);
    
    // Cache management
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
    QString m_cacheDir;
    QSettings* m_cacheIndex;
    QList<CachedFitsImage> m_cachedImages;
    
    // Standard fetch size
    static constexpr double FETCH_SIZE_ARCMIN = 60.0;
    
    // Composite tracking
    struct CompositeRequest {
        double ra_deg, dec_deg;
        double width_arcmin, height_arcmin;
        QString cacheKey;
        QByteArray irFits, redFits, blueFits;
        int completedCount;
        bool active;
    };
    CompositeRequest m_compositeRequest;
    
    // Cache management
    void loadCacheIndex();
    void saveCacheIndex();
    CachedFitsImage* findCachedImageContaining(double ra_deg, double dec_deg);
    QString generateCacheKey(double ra_deg, double dec_deg);
    void addToCacheIndex(const CachedFitsImage& image);
    
    // Image processing
    QByteArray cropAndCreateTiff(const CachedFitsImage& cached, 
                                 double target_ra_deg, double target_dec_deg);
    QByteArray createRGBTiffFromFits(const QByteArray& irFits,
                                     const QByteArray& redFits,
                                     const QByteArray& blueFits,
                                     double ra_deg, double dec_deg,
                                     double width_arcmin, double height_arcmin);
    QImage parseFitsToImage(const QByteArray& fitsData);
    QImage cropFitsImage(const QImage& fullImage, 
                        double full_center_ra, double full_center_dec,
                        double full_width_arcmin, double full_height_arcmin,
                        double crop_center_ra, double crop_center_dec,
                        double crop_width_arcmin, double crop_height_arcmin);
    
    // Network
    void fetchNewComposite(double ra_deg, double dec_deg);
    QString buildDSSUrl(double ra_deg, double dec_deg, 
                       double width_arcmin, double height_arcmin,
                       DSSurvey survey);
    
    QString surveyToString(DSSurvey survey) const;
    QByteArray createRGBTiffFromImages(const QImage& irImage,
						       const QImage& redImage,
						       const QImage& blueImage,
						       double ra_deg, double dec_deg);
};

#endif // DSSFITSMANAGER_H
