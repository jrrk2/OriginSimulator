#include "DSSFitsManager.h"
#include <QNetworkRequest>
#include <QUrl>
#include <QFile>
#include <QCryptographicHash>
#include <QPainter>
#include <QBuffer>
#include <QDebug>
#include <algorithm>
#include <vector>
#include <cmath>

DSSFitsManager::DSSFitsManager(QObject *parent) 
    : QObject(parent) {
    
    m_networkManager = new QNetworkAccessManager(this);
    
    // Setup cache directory
    QString homeDir = QDir::homePath();
    m_cacheDir = QDir(homeDir).absoluteFilePath(
        "Library/Application Support/OriginSimulator/Images/DSS_Cache"
    );
    QDir().mkpath(m_cacheDir);
    
    // Setup persistent cache index
    QString indexPath = QDir(m_cacheDir).absoluteFilePath("cache_index.ini");
    m_cacheIndex = new QSettings(indexPath, QSettings::IniFormat, this);
    
    m_compositeRequest.active = false;
    
    // Load existing cache
    loadCacheIndex();
    
    qDebug() << "DSS FITS Manager initialized";
    qDebug() << "Cache directory:" << m_cacheDir;
    qDebug() << "Cached images:" << m_cachedImages.size();
}

void DSSFitsManager::fetchImageForPosition(double ra_deg, double dec_deg) {
    qDebug() << QString("Request for RA=%1°, Dec=%2°")
                .arg(ra_deg, 0, 'f', 6).arg(dec_deg, 0, 'f', 6);
    
    // Check if we have a cached image that contains this position
    CachedFitsImage* cached = findCachedImageContaining(ra_deg, dec_deg);
    
    if (cached && cached->isValid()) {
        // Cache hit - crop from existing data
        QString info = QString("Cache HIT: Using cached image centered at RA=%1°, Dec=%2°")
                      .arg(cached->center_ra_deg, 0, 'f', 4)
                      .arg(cached->center_dec_deg, 0, 'f', 4);
        qDebug() << info;
        emit cacheHit(info);
        
        QByteArray tiff = cropAndCreateTiff(*cached, ra_deg, dec_deg);
        if (!tiff.isEmpty()) {
            emit imageReady(tiff);
            return;
        }
        
        qDebug() << "Crop failed, will fetch new image";
    }
    
    // Cache miss - fetch new composite
    QString info = QString("Cache MISS: Fetching new 60x60' composite centered at RA=%1°, Dec=%2°")
                  .arg(ra_deg, 0, 'f', 4).arg(dec_deg, 0, 'f', 4);
    qDebug() << info;
    emit cacheMiss(info);
    
    fetchNewComposite(ra_deg, dec_deg);
}

void DSSFitsManager::fetchNewComposite(double ra_deg, double dec_deg) {
    m_compositeRequest.ra_deg = ra_deg;
    m_compositeRequest.dec_deg = dec_deg;
    m_compositeRequest.width_arcmin = FETCH_SIZE_ARCMIN;
    m_compositeRequest.height_arcmin = FETCH_SIZE_ARCMIN;
    m_compositeRequest.cacheKey = generateCacheKey(ra_deg, dec_deg);
    m_compositeRequest.completedCount = 0;
    m_compositeRequest.active = true;
    
    qDebug() << QString("Fetching new 60x60' DSS composite");
    
    // Fetch all three bands
    QList<DSSurvey> surveys = {
        DSSurvey::POSS2UKSTU_IR,
        DSSurvey::POSS2UKSTU_RED,
        DSSurvey::POSS2UKSTU_BLUE
    };
    
    for (DSSurvey survey : surveys) {
        QString url = buildDSSUrl(ra_deg, dec_deg, 
                                 FETCH_SIZE_ARCMIN, FETCH_SIZE_ARCMIN, survey);
        
        if (url.isEmpty()) {
            emit fetchError("Failed to build DSS URL");
            continue;
        }
        
        qDebug() << "Fetching:" << surveyToString(survey);
        
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::UserAgentHeader, "OriginSimulator/1.0");
        
        QNetworkReply* reply = m_networkManager->get(request);
        
        reply->setProperty("ra_deg", ra_deg);
        reply->setProperty("dec_deg", dec_deg);
        reply->setProperty("survey", (int)survey);
        reply->setProperty("cache_key", m_compositeRequest.cacheKey);
        
        connect(reply, &QNetworkReply::finished, this, &DSSFitsManager::onNetworkReply);
    }
}

void DSSFitsManager::onNetworkReply() {
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    double ra_deg = reply->property("ra_deg").toDouble();
    double dec_deg = reply->property("dec_deg").toDouble();
    DSSurvey survey = (DSSurvey)reply->property("survey").toInt();
    QString cacheKey = reply->property("cache_key").toString();
    
    if (reply->error() != QNetworkReply::NoError) {
        QString error = QString("DSS fetch failed: %1").arg(reply->errorString());
        qDebug() << error;
        emit fetchError(error);
        reply->deleteLater();
        return;
    }
    
    QByteArray fitsData = reply->readAll();
    reply->deleteLater();
    
    if (fitsData.isEmpty()) {
        emit fetchError("Empty FITS data received");
        return;
    }
    
    qDebug() << "Received" << surveyToString(survey) << ":" << fitsData.size() << "bytes";
    
    // Store FITS data
    if (m_compositeRequest.active) {
        if (survey == DSSurvey::POSS2UKSTU_IR) {
            m_compositeRequest.irFits = fitsData;
        } else if (survey == DSSurvey::POSS2UKSTU_RED) {
            m_compositeRequest.redFits = fitsData;
        } else if (survey == DSSurvey::POSS2UKSTU_BLUE) {
            m_compositeRequest.blueFits = fitsData;
        }
        
        m_compositeRequest.completedCount++;
        
        if (m_compositeRequest.completedCount == 3) {
            qDebug() << "All bands received, creating composite and caching";
            
            // Save FITS files to disk
            CachedFitsImage cached;
            cached.cacheKey = cacheKey;
            cached.center_ra_deg = ra_deg;
            cached.center_dec_deg = dec_deg;
            cached.width_arcmin = FETCH_SIZE_ARCMIN;
            cached.height_arcmin = FETCH_SIZE_ARCMIN;
            cached.fetchTime = QDateTime::currentDateTime();
            
            cached.irFilePath = QString("%1/%2_ir.fits").arg(m_cacheDir).arg(cacheKey);
            cached.redFilePath = QString("%1/%2_red.fits").arg(m_cacheDir).arg(cacheKey);
            cached.blueFilePath = QString("%1/%2_blue.fits").arg(m_cacheDir).arg(cacheKey);
            
            // Write FITS to disk
            QFile irFile(cached.irFilePath);
            if (irFile.open(QIODevice::WriteOnly)) {
                irFile.write(m_compositeRequest.irFits);
                irFile.close();
            }
            
            QFile redFile(cached.redFilePath);
            if (redFile.open(QIODevice::WriteOnly)) {
                redFile.write(m_compositeRequest.redFits);
                redFile.close();
            }
            
            QFile blueFile(cached.blueFilePath);
            if (blueFile.open(QIODevice::WriteOnly)) {
                blueFile.write(m_compositeRequest.blueFits);
                blueFile.close();
            }
            
            qDebug() << "Saved FITS to cache:" << cacheKey;
            
            // Add to cache index
            addToCacheIndex(cached);
            
            // Create RGB TIFF for current position (centered)
            QByteArray tiff = createRGBTiffFromFits(
                m_compositeRequest.irFits,
                m_compositeRequest.redFits,
                m_compositeRequest.blueFits,
                ra_deg, dec_deg,
                FETCH_SIZE_ARCMIN, FETCH_SIZE_ARCMIN
            );
            
            m_compositeRequest.active = false;
            
            if (!tiff.isEmpty()) {
                emit imageReady(tiff);
            } else {
                emit fetchError("Failed to create TIFF");
            }
        }
    }
}

QByteArray DSSFitsManager::cropAndCreateTiff(const CachedFitsImage& cached,
                                             double target_ra_deg, double target_dec_deg) {
    
    qDebug() << "Cropping from cached image";
    
    // Load FITS files from disk
    QFile irFile(cached.irFilePath);
    QFile redFile(cached.redFilePath);
    QFile blueFile(cached.blueFilePath);
    
    if (!irFile.open(QIODevice::ReadOnly) ||
        !redFile.open(QIODevice::ReadOnly) ||
        !blueFile.open(QIODevice::ReadOnly)) {
        qDebug() << "Failed to open cached FITS files";
        return QByteArray();
    }
    
    QByteArray irFits = irFile.readAll();
    QByteArray redFits = redFile.readAll();
    QByteArray blueFits = blueFile.readAll();
    
    irFile.close();
    redFile.close();
    blueFile.close();
    
    // Parse to images
    QImage irImage = parseFitsToImage(irFits);
    QImage redImage = parseFitsToImage(redFits);
    QImage blueImage = parseFitsToImage(blueFits);
    
    if (irImage.isNull() || redImage.isNull() || blueImage.isNull()) {
        qDebug() << "Failed to parse cached FITS";
        return QByteArray();
    }
    
    // Mirror vertically
    irImage = irImage.mirrored(false, true);
    redImage = redImage.mirrored(false, true);
    blueImage = blueImage.mirrored(false, true);
    
    // Crop to target position (20x20 arcmin for display)
    double crop_size = 20.0;
    
    QImage irCropped = cropFitsImage(irImage, 
                                     cached.center_ra_deg, cached.center_dec_deg,
                                     cached.width_arcmin, cached.height_arcmin,
                                     target_ra_deg, target_dec_deg,
                                     crop_size, crop_size);
    
    QImage redCropped = cropFitsImage(redImage,
                                      cached.center_ra_deg, cached.center_dec_deg,
                                      cached.width_arcmin, cached.height_arcmin,
                                      target_ra_deg, target_dec_deg,
                                      crop_size, crop_size);
    
    QImage blueCropped = cropFitsImage(blueImage,
                                       cached.center_ra_deg, cached.center_dec_deg,
                                       cached.width_arcmin, cached.height_arcmin,
                                       target_ra_deg, target_dec_deg,
                                       crop_size, crop_size);
    
    if (irCropped.isNull() || redCropped.isNull() || blueCropped.isNull()) {
        qDebug() << "Crop failed";
        return QByteArray();
    }
    
    qDebug() << "Cropped successfully";
    
    // Create RGB TIFF from cropped images
    return createRGBTiffFromImages(irCropped, redCropped, blueCropped, 
                                   target_ra_deg, target_dec_deg);
}

QImage DSSFitsManager::cropFitsImage(const QImage& fullImage,
                                     double full_center_ra, double full_center_dec,
                                     double full_width_arcmin, double full_height_arcmin,
                                     double crop_center_ra, double crop_center_dec,
                                     double crop_width_arcmin, double crop_height_arcmin) {
    
    int fullWidth = fullImage.width();
    int fullHeight = fullImage.height();
    
    // Calculate pixel scale
    double arcsecPerPixelX = (full_width_arcmin * 60.0) / fullWidth;
    double arcsecPerPixelY = (full_height_arcmin * 60.0) / fullHeight;
    
    // Calculate offset from center in arcseconds
    double deltaRA = (crop_center_ra - full_center_ra) * cos(full_center_dec * M_PI / 180.0);
    double deltaDec = crop_center_dec - full_center_dec;
    
    double offsetRA_arcsec = deltaRA * 3600.0;
    double offsetDec_arcsec = deltaDec * 3600.0;
    
    // Convert to pixels
    int offsetX_pixels = static_cast<int>(offsetRA_arcsec / arcsecPerPixelX);
    int offsetY_pixels = static_cast<int>(-offsetDec_arcsec / arcsecPerPixelY); // Y is flipped
    
    // Calculate crop size in pixels
    int cropWidth_pixels = static_cast<int>((crop_width_arcmin * 60.0) / arcsecPerPixelX);
    int cropHeight_pixels = static_cast<int>((crop_height_arcmin * 60.0) / arcsecPerPixelY);
    
    // Calculate crop rectangle centered on target
    int cropX = (fullWidth / 2) + offsetX_pixels - (cropWidth_pixels / 2);
    int cropY = (fullHeight / 2) + offsetY_pixels - (cropHeight_pixels / 2);
    
    qDebug() << QString("Crop: offset=(%1,%2)px, size=%3x%4px, pos=(%5,%6)")
                .arg(offsetX_pixels).arg(offsetY_pixels)
                .arg(cropWidth_pixels).arg(cropHeight_pixels)
                .arg(cropX).arg(cropY);
    
    // Clamp to image bounds
    if (cropX < 0) cropX = 0;
    if (cropY < 0) cropY = 0;
    if (cropX + cropWidth_pixels > fullWidth) cropX = fullWidth - cropWidth_pixels;
    if (cropY + cropHeight_pixels > fullHeight) cropY = fullHeight - cropHeight_pixels;
    
    if (cropX < 0 || cropY < 0 || 
        cropX + cropWidth_pixels > fullWidth || 
        cropY + cropHeight_pixels > fullHeight) {
        qDebug() << "Crop rectangle outside image bounds";
        return QImage();
    }
    
    QRect cropRect(cropX, cropY, cropWidth_pixels, cropHeight_pixels);
    return fullImage.copy(cropRect);
}

QByteArray DSSFitsManager::createRGBTiffFromImages(const QImage& irImage,
                                                   const QImage& redImage,
                                                   const QImage& blueImage,
                                                   double ra_deg, double dec_deg) {
    
    // Target size for Origin telescope
    int targetWidth = 800;
    int targetHeight = 600;
    
    // Scale all images
    QImage ir = irImage.scaled(targetWidth, targetHeight, 
                                Qt::KeepAspectRatio, Qt::SmoothTransformation)
                       .convertToFormat(QImage::Format_RGB32);
    QImage red = redImage.scaled(targetWidth, targetHeight,
                                 Qt::KeepAspectRatio, Qt::SmoothTransformation)
                        .convertToFormat(QImage::Format_RGB32);
    QImage blue = blueImage.scaled(targetWidth, targetHeight,
                                   Qt::KeepAspectRatio, Qt::SmoothTransformation)
                          .convertToFormat(QImage::Format_RGB32);
    
    // Create false color composite
    QImage composite(targetWidth, targetHeight, QImage::Format_RGB32);
    composite.fill(Qt::black);
    
    int srcWidth = ir.width();
    int srcHeight = ir.height();
    int offsetX = (targetWidth - srcWidth) / 2;
    int offsetY = (targetHeight - srcHeight) / 2;
    
    for (int y = 0; y < srcHeight; ++y) {
        QRgb* compositeLine = reinterpret_cast<QRgb*>(composite.scanLine(y + offsetY));
        const QRgb* irLine = reinterpret_cast<const QRgb*>(ir.constScanLine(y));
        const QRgb* redLine = reinterpret_cast<const QRgb*>(red.constScanLine(y));
        const QRgb* blueLine = reinterpret_cast<const QRgb*>(blue.constScanLine(y));
        
        for (int x = 0; x < srcWidth; ++x) {
            int irValue = qRed(irLine[x]);
            int redValue = qRed(redLine[x]);
            int blueValue = qRed(blueLine[x]);
            
            compositeLine[x + offsetX] = qRgb(irValue, redValue, blueValue);
        }
    }
    
    // Add overlay
    QPainter painter(&composite);
    painter.setPen(QPen(Qt::yellow, 2));
    int centerX = targetWidth / 2;
    int centerY = targetHeight / 2;
    painter.drawLine(centerX - 30, centerY, centerX + 30, centerY);
    painter.drawLine(centerX, centerY - 30, centerX, centerY + 30);
    
    painter.setFont(QFont("Arial", 10, QFont::Bold));
    painter.setPen(Qt::white);
    QString coordText = QString("RA: %1° Dec: %2°")
                       .arg(ra_deg, 0, 'f', 3).arg(dec_deg, 0, 'f', 3);
    painter.drawText(10, 20, coordText);
    
    QString timeText = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss UTC");
    painter.drawText(10, targetHeight - 10, timeText);
    
    painter.setPen(Qt::green);
    painter.setFont(QFont("Arial", 8, QFont::Bold));
    painter.drawText(targetWidth - 120, 20, "REAL DSS DATA");
    
    painter.end();
    
    // Convert to TIFF
    QByteArray tiffData;
    QBuffer buffer(&tiffData);
    buffer.open(QIODevice::WriteOnly);
    bool success = composite.save(&buffer, "TIFF", 100);
    buffer.close();
    
    if (!success) {
        qDebug() << "Failed to convert to TIFF";
        return QByteArray();
    }
    
    return tiffData;
}

QByteArray DSSFitsManager::createRGBTiffFromFits(const QByteArray& irFits,
                                                 const QByteArray& redFits,
                                                 const QByteArray& blueFits,
                                                 double ra_deg, double dec_deg,
                                                 double width_arcmin, double height_arcmin) {
    
    QImage irImage = parseFitsToImage(irFits);
    QImage redImage = parseFitsToImage(redFits);
    QImage blueImage = parseFitsToImage(blueFits);
    
    if (irImage.isNull() || redImage.isNull() || blueImage.isNull()) {
        return QByteArray();
    }
    
    // Mirror vertically
    irImage = irImage.mirrored(false, true);
    redImage = redImage.mirrored(false, true);
    blueImage = blueImage.mirrored(false, true);
    
    return createRGBTiffFromImages(irImage, redImage, blueImage, ra_deg, dec_deg);
}

// Cache management methods

void DSSFitsManager::loadCacheIndex() {
    m_cachedImages.clear();
    
    int size = m_cacheIndex->beginReadArray("cached_images");
    for (int i = 0; i < size; ++i) {
        m_cacheIndex->setArrayIndex(i);
        
        CachedFitsImage cached;
        cached.cacheKey = m_cacheIndex->value("key").toString();
        cached.center_ra_deg = m_cacheIndex->value("ra").toDouble();
        cached.center_dec_deg = m_cacheIndex->value("dec").toDouble();
        cached.width_arcmin = m_cacheIndex->value("width").toDouble();
        cached.height_arcmin = m_cacheIndex->value("height").toDouble();
        cached.irFilePath = m_cacheIndex->value("ir_path").toString();
        cached.redFilePath = m_cacheIndex->value("red_path").toString();
        cached.blueFilePath = m_cacheIndex->value("blue_path").toString();
        cached.fetchTime = m_cacheIndex->value("fetch_time").toDateTime();
        
        if (cached.isValid()) {
            m_cachedImages.append(cached);
            qDebug() << QString("Loaded cache entry: RA=%1, Dec=%2, %3x%4'")
                        .arg(cached.center_ra_deg, 0, 'f', 2)
                        .arg(cached.center_dec_deg, 0, 'f', 2)
                        .arg(cached.width_arcmin, 0, 'f', 0)
                        .arg(cached.height_arcmin, 0, 'f', 0);
        }
    }
    m_cacheIndex->endArray();
}

void DSSFitsManager::saveCacheIndex() {
    m_cacheIndex->beginWriteArray("cached_images");
    for (int i = 0; i < m_cachedImages.size(); ++i) {
        m_cacheIndex->setArrayIndex(i);
        const CachedFitsImage& cached = m_cachedImages[i];
        
        m_cacheIndex->setValue("key", cached.cacheKey);
        m_cacheIndex->setValue("ra", cached.center_ra_deg);
        m_cacheIndex->setValue("dec", cached.center_dec_deg);
        m_cacheIndex->setValue("width", cached.width_arcmin);
        m_cacheIndex->setValue("height", cached.height_arcmin);
        m_cacheIndex->setValue("ir_path", cached.irFilePath);
        m_cacheIndex->setValue("red_path", cached.redFilePath);
        m_cacheIndex->setValue("blue_path", cached.blueFilePath);
        m_cacheIndex->setValue("fetch_time", cached.fetchTime);
    }
    m_cacheIndex->endArray();
    m_cacheIndex->sync();
}

CachedFitsImage* DSSFitsManager::findCachedImageContaining(double ra_deg, double dec_deg) {
    for (CachedFitsImage& cached : m_cachedImages) {
        if (cached.containsPosition(ra_deg, dec_deg)) {
            return &cached;
        }
    }
    return nullptr;
}

QString DSSFitsManager::generateCacheKey(double ra_deg, double dec_deg) {
    QString key = QString("ra%1_dec%2")
                  .arg(ra_deg, 0, 'f', 4)
                  .arg(dec_deg, 0, 'f', 4);
    
    QByteArray hash = QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Md5);
    return QString(hash.toHex().left(16));
}

void DSSFitsManager::addToCacheIndex(const CachedFitsImage& image) {
    m_cachedImages.append(image);
    saveCacheIndex();
    
    qDebug() << QString("Added to cache: RA=%1, Dec=%2, coverage=%3x%4'")
                .arg(image.center_ra_deg, 0, 'f', 2)
                .arg(image.center_dec_deg, 0, 'f', 2)
                .arg(image.width_arcmin, 0, 'f', 0)
                .arg(image.height_arcmin, 0, 'f', 0);
}

QImage DSSFitsManager::parseFitsToImage(const QByteArray& fitsData) {
    if (fitsData.isEmpty()) return QImage();
    
    fitsfile *fptr = nullptr;
    int status = 0;
    
    QByteArray mutableData = fitsData;
    size_t memsize = mutableData.size();
    void *(*mem_realloc)(void *p, size_t newsize) = nullptr;
    char *data = mutableData.data();
    
    if (fits_open_memfile(&fptr, "memory.fits", READONLY,
                         (void**)&data, &memsize, 0, mem_realloc, &status)) {
        fits_report_error(stderr, status);
        return QImage();
}
    
    int hdutype = 0;
    fits_get_hdu_type(fptr, &hdutype, &status);
    if (hdutype != IMAGE_HDU) {
        fits_close_file(fptr, &status);
        return QImage();
    }
    
    int bitpix = 0, naxis = 0;
    long naxes[3] = {1, 1, 1};
    fits_get_img_param(fptr, 3, &bitpix, &naxis, naxes, &status);
    
    if (naxis < 2) {
        fits_close_file(fptr, &status);
        return QImage();
    }
    
    const int width = naxes[0];
    const int height = naxes[1];
    const long npixels = width * height;
    
    std::vector<float> buffer(npixels);
    long fpixel[3] = {1, 1, 1};
    
    if (fits_read_pix(fptr, TFLOAT, fpixel, npixels, NULL, buffer.data(), NULL, &status)) {
        fits_report_error(stderr, status);
        fits_close_file(fptr, &status);
        return QImage();
    }
    
    fits_close_file(fptr, &status);
    
    // Compute min/max for scaling
    auto [minIt, maxIt] = std::minmax_element(buffer.begin(), buffer.end());
    float minVal = *minIt;
    float maxVal = *maxIt;
    
    if (minVal == maxVal) maxVal = minVal + 1.0f;
    
    const float scale = 255.0f / (maxVal - minVal);
    
    // Create grayscale image
    QImage img(width, height, QImage::Format_Grayscale8);
    
    for (int y = 0; y < height; ++y) {
        uchar *scan = img.scanLine(y);
        for (int x = 0; x < width; ++x) {
            float v = buffer[y * width + x];
            int scaled = int((v - minVal) * scale + 0.5f);
            if (scaled < 0) scaled = 0;
            if (scaled > 255) scaled = 255;
            scan[x] = uchar(scaled);
        }
    }
    
    return img;
}

QString DSSFitsManager::buildDSSUrl(double ra_deg, double dec_deg,
                                   double width_arcmin, double height_arcmin,
                                   DSSurvey survey) {
    
    QString baseUrl = "http://archive.stsci.edu/cgi-bin/dss_search";
    
    QMap<DSSurvey, QString> surveyCodes = {
        {DSSurvey::POSS2UKSTU_RED, "poss2ukstu_red"},
        {DSSurvey::POSS2UKSTU_BLUE, "poss2ukstu_blue"},
        {DSSurvey::POSS2UKSTU_IR, "poss2ukstu_ir"},
        {DSSurvey::POSS1_RED, "poss1_red"},
        {DSSurvey::POSS1_BLUE, "poss1_blue"},
        {DSSurvey::QUICKV, "quickv"}
    };
    
    QString surveyCode = surveyCodes.value(survey, "poss2ukstu_red");
    
    QString url = QString("%1?v=%2&r=%3&d=%4&e=J2000&h=%5&w=%6&f=fits&c=none&fov=NONE&v3=")
                  .arg(baseUrl)
                  .arg(surveyCode)
                  .arg(ra_deg, 0, 'f', 6)
                  .arg(dec_deg, 0, 'f', 6)
                  .arg(height_arcmin, 0, 'f', 2)
                  .arg(width_arcmin, 0, 'f', 2);
    
    return url;
}

QString DSSFitsManager::surveyToString(DSSurvey survey) const {
    switch (survey) {
        case DSSurvey::POSS2UKSTU_RED: return "Red";
        case DSSurvey::POSS2UKSTU_BLUE: return "Blue";
        case DSSurvey::POSS2UKSTU_IR: return "IR";
        case DSSurvey::POSS1_RED: return "POSS1_Red";
        case DSSurvey::POSS1_BLUE: return "POSS1_Blue";
        case DSSurvey::QUICKV: return "QuickV";
        default: return "Unknown";
    }
}

void DSSFitsManager::clearCache() {
    // Delete all cached FITS files
    for (const CachedFitsImage& cached : m_cachedImages) {
        QFile::remove(cached.irFilePath);
        QFile::remove(cached.redFilePath);
        QFile::remove(cached.blueFilePath);
    }
    
    m_cachedImages.clear();
    saveCacheIndex();
    
    qDebug() << "Cache cleared";
}

qint64 DSSFitsManager::getCacheSize() const {
    qint64 size = 0;
    for (const CachedFitsImage& cached : m_cachedImages) {
        QFileInfo irInfo(cached.irFilePath);
        QFileInfo redInfo(cached.redFilePath);
        QFileInfo blueInfo(cached.blueFilePath);
        
        size += irInfo.size() + redInfo.size() + blueInfo.size();
    }
    return size;
}
