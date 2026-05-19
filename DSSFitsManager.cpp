#include "DSSFitsManager.h"
#include "GaiaStarFieldRenderer.h"
#include <QNetworkRequest>
#include <QUrl>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <algorithm>
#include <cmath>

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

DSSFitsManager::DSSFitsManager(QObject *parent)
    : QObject(parent)
{
    m_networkManager = new QNetworkAccessManager(this);
    m_gaiaRenderer = new GaiaStarFieldRenderer(this);

    QString homeDir = QDir::homePath();
    m_cacheDir = QDir(homeDir).absoluteFilePath(
        "Library/Application Support/OriginSimulator/Images/Legacy_Cache"
    );
    QDir().mkpath(m_cacheDir);

    QString indexPath = QDir(m_cacheDir).absoluteFilePath("cache_index.ini");
    m_cacheIndex = new QSettings(indexPath, QSettings::IniFormat, this);

    m_pending.active = false;

    loadCacheIndex();

    qDebug() << "Legacy Survey Manager initialized";
    qDebug() << "Cache directory:" << m_cacheDir;
    qDebug() << "Cached images:" << m_cachedImages.size();
}

// ---------------------------------------------------------------------------
// Main entry point
// ---------------------------------------------------------------------------

void DSSFitsManager::fetchImageForPosition(double ra_deg, double dec_deg)
{
    qDebug() << QString("Request for RA=%1, Dec=%2")
                .arg(ra_deg, 0, 'f', 6).arg(dec_deg, 0, 'f', 6);

    // Check cache
    CachedFitsImage *cached = findCachedTileContaining(ra_deg, dec_deg);
    if (cached && cached->isValid()) {
        QString info = QString("Cache HIT: tile centered at RA=%1, Dec=%2")
                      .arg(cached->center_ra_deg, 0, 'f', 4)
                      .arg(cached->center_dec_deg, 0, 'f', 4);
        qDebug() << info;
        emit cacheHit(info);

        QByteArray tiff = processFitsTile(cached->fitsFilePath, ra_deg, dec_deg,
                                           cached->center_ra_deg, cached->center_dec_deg);
        if (!tiff.isEmpty()) {
            emit imageReady(tiff);
            return;
        }
        qDebug() << "Processing cached tile failed, will re-fetch";
    }

    // Cache miss — fetch from Legacy Survey
    // Round coordinates to nearest 100" (1/36 deg) for cache key reuse
    double cacheRA  = std::round(ra_deg  * 36.0) / 36.0;
    double cacheDec = std::round(dec_deg * 36.0) / 36.0;

    QString cacheKey = QString("ls_%1_%2_%3")
                      .arg(cacheRA, 0, 'f', 4)
                      .arg(cacheDec, 0, 'f', 4)
                      .arg(PIXSCALE_ARCSEC, 0, 'f', 3);
    QString cachePath = QString("%1/%2.fits").arg(m_cacheDir).arg(cacheKey);

    // Check if this exact cache file exists on disk but wasn't in the index
    QFileInfo diskCheck(cachePath);
    if (diskCheck.exists() && diskCheck.size() > 2880) {
        qDebug() << "Found un-indexed cache file:" << cachePath;
        QByteArray tiff = processFitsTile(cachePath, ra_deg, dec_deg, cacheRA, cacheDec);
        if (!tiff.isEmpty()) {
            // Add to index
            CachedFitsImage entry;
            entry.cacheKey = cacheKey;
            entry.center_ra_deg = cacheRA;
            entry.center_dec_deg = cacheDec;
            entry.width_arcmin  = FETCH_SIZE_PX * PIXSCALE_ARCSEC / 60.0;
            entry.height_arcmin = FETCH_SIZE_PX * PIXSCALE_ARCSEC / 60.0;
            entry.fitsFilePath = cachePath;
            entry.fetchTime = diskCheck.lastModified();
            addToCacheIndex(entry);

            emit cacheHit("Using un-indexed cached tile");
            emit imageReady(tiff);
            return;
        }
    }

    QString info = QString("Cache MISS: Fetching Legacy Survey tile at RA=%1, Dec=%2")
                  .arg(cacheRA, 0, 'f', 4).arg(cacheDec, 0, 'f', 4);
    qDebug() << info;
    emit cacheMiss(info);

    // Build URL
    // Legacy Survey cutout service: returns 3D FITS cube (g, r, z bands)
    QString url = QString(
        "https://www.legacysurvey.org/viewer/cutout.fits"
        "?ra=%1&dec=%2&size=%3&layer=ls-dr10&pixscale=%4")
        .arg(cacheRA, 0, 'f', 6)
        .arg(cacheDec, 0, 'f', 6)
        .arg(FETCH_SIZE_PX)
        .arg(PIXSCALE_ARCSEC, 0, 'f', 3);

    qDebug() << "Fetching:" << url;

    m_pending.ra_deg = ra_deg;
    m_pending.dec_deg = dec_deg;
    m_pending.cache_ra = cacheRA;
    m_pending.cache_dec = cacheDec;
    m_pending.cacheKey = cacheKey;
    m_pending.cachePath = cachePath;
    m_pending.active = true;

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "OriginSimulator/2.0");
    request.setTransferTimeout(120000);  // 120s timeout

    QNetworkReply *reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &DSSFitsManager::onNetworkReply);
}

// ---------------------------------------------------------------------------
// Network reply handler
// ---------------------------------------------------------------------------

void DSSFitsManager::onNetworkReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    reply->deleteLater();

    if (!m_pending.active) return;

    if (reply->error() != QNetworkReply::NoError) {
        QString error = QString("Legacy Survey fetch failed: %1").arg(reply->errorString());
        qDebug() << error;
        m_pending.active = false;
        emit fetchError(error);
        return;
    }

    QByteArray fitsData = reply->readAll();
    if (fitsData.size() < 2880) {
        m_pending.active = false;
        emit fetchError("Received too-small FITS response");
        return;
    }

    qDebug() << "Received Legacy Survey FITS:" << fitsData.size() << "bytes";

    // Save to cache
    QFile cacheFile(m_pending.cachePath);
    if (cacheFile.open(QIODevice::WriteOnly)) {
        cacheFile.write(fitsData);
        cacheFile.close();
        qDebug() << "Saved to cache:" << m_pending.cachePath;
    }

    // Add to cache index
    CachedFitsImage entry;
    entry.cacheKey = m_pending.cacheKey;
    entry.center_ra_deg = m_pending.cache_ra;
    entry.center_dec_deg = m_pending.cache_dec;
    entry.width_arcmin  = FETCH_SIZE_PX * PIXSCALE_ARCSEC / 60.0;
    entry.height_arcmin = FETCH_SIZE_PX * PIXSCALE_ARCSEC / 60.0;
    entry.fitsFilePath = m_pending.cachePath;
    entry.fetchTime = QDateTime::currentDateTime();
    addToCacheIndex(entry);

    // Process and emit
    QByteArray tiff = processFitsTile(m_pending.cachePath,
                                       m_pending.ra_deg, m_pending.dec_deg,
                                       m_pending.cache_ra, m_pending.cache_dec);
    m_pending.active = false;

    if (!tiff.isEmpty()) {
        emit imageReady(tiff);
    } else {
        emit fetchError("Failed to process Legacy Survey FITS");
    }
}

// ---------------------------------------------------------------------------
// FITS cube processing: read g/r/z bands, normalize, crop, write 16-bit TIFF
// ---------------------------------------------------------------------------

QByteArray DSSFitsManager::processFitsTile(const QString &fitsPath,
                                            double target_ra, double target_dec,
                                            double tile_center_ra, double tile_center_dec)
{
    fitsfile *fptr = nullptr;
    int status = 0;

    fits_open_file(&fptr, fitsPath.toUtf8().constData(), READONLY, &status);
    if (status) {
        qWarning() << "Failed to open FITS:" << fitsPath;
        return QByteArray();
    }

    int naxis = 0;
    long naxes[3] = {1, 1, 1};
    fits_get_img_dim(fptr, &naxis, &status);
    fits_get_img_size(fptr, 3, naxes, &status);

    int tileW = (int)naxes[0];
    int tileH = (int)naxes[1];
    int nBands = (naxis >= 3) ? (int)naxes[2] : 1;

    qDebug() << "FITS tile:" << tileW << "x" << tileH << "(" << nBands << "bands)";

    if (tileW < OUTPUT_WIDTH || tileH < OUTPUT_HEIGHT) {
        qWarning() << "FITS tile too small for output";
        fits_close_file(fptr, &status);
        return QByteArray();
    }

    // Read all band planes
    int npix = tileW * tileH;
    std::vector<std::vector<float>> bands(nBands, std::vector<float>(npix, 0.0f));

    if (naxis >= 3) {
        for (int b = 0; b < nBands; b++) {
            long fpixel[3] = {1, 1, b + 1};
            fits_read_pix(fptr, TFLOAT, fpixel, npix, nullptr,
                          bands[b].data(), nullptr, &status);
            if (status) {
                qWarning() << "Error reading band" << b;
                fits_close_file(fptr, &status);
                return QByteArray();
            }
        }
    } else {
        long fpixel[2] = {1, 1};
        fits_read_pix(fptr, TFLOAT, fpixel, npix, nullptr,
                      bands[0].data(), nullptr, &status);
    }
    fits_close_file(fptr, &status);

    // Map bands to RGB.  Legacy Survey DR10 returns g,r,i,z (4 bands)
    // or g,r,z (3 bands).  Map: z->R, r->G, g->B for visual color.
    std::vector<float> &srcR = (nBands >= 4) ? bands[3] : (nBands >= 3) ? bands[2] : bands[0];
    std::vector<float> &srcG = (nBands >= 2) ? bands[1] : bands[0];
    std::vector<float> &srcB = bands[0];

    // Replace NaNs with 0 and count non-zero pixels
    int nonZeroCount = 0;
    for (int i = 0; i < npix; i++) {
        if (std::isnan(srcR[i])) srcR[i] = 0;
        if (std::isnan(srcG[i])) srcG[i] = 0;
        if (std::isnan(srcB[i])) srcB[i] = 0;
        if (srcR[i] != 0 || srcG[i] != 0 || srcB[i] != 0) nonZeroCount++;
    }

    // If no survey coverage, fall back to Gaia star catalog rendering,
    // then to a synthetic star field as last resort.  We must always return
    // a valid TIFF so the snapshot flow completes and the client doesn't hang.
    if (nonZeroCount == 0) {
        qWarning() << "No Legacy Survey coverage at this position — trying Gaia catalog";

        if (m_gaiaRenderer && m_gaiaRenderer->isAvailable()) {
            QByteArray gaiaTiff = m_gaiaRenderer->renderField(
                target_ra, target_dec,
                OUTPUT_WIDTH, OUTPUT_HEIGHT, PIXSCALE_ARCSEC);
            if (!gaiaTiff.isEmpty()) {
                qDebug() << "Gaia fallback rendered" << gaiaTiff.size() << "bytes";
                return gaiaTiff;
            }
        }

        // Last resort: synthetic star field so the snapshot always completes
        qWarning() << "Generating synthetic star field as last resort";
        unsigned seed = (unsigned)(fabs(target_ra * 1000) + fabs(target_dec * 1000));
        srand(seed);

        int outN = OUTPUT_WIDTH * OUTPUT_HEIGHT;
        std::vector<float> outR(outN), outG(outN), outB(outN);

        for (int i = 0; i < outN; i++) {
            float noise = (rand() % 100) / 200000.0f;
            outR[i] = noise; outG[i] = noise; outB[i] = noise;
        }
        int numStars = 30 + (rand() % 20);
        for (int s = 0; s < numStars; s++) {
            int cx = rand() % OUTPUT_WIDTH;
            int cy = rand() % OUTPUT_HEIGHT;
            float brightness = 0.1f + (rand() % 900) / 1000.0f;
            float sigma = 1.5f + (rand() % 30) / 10.0f;
            int radius = (int)(sigma * 3);
            for (int dy = -radius; dy <= radius; dy++) {
                for (int dx = -radius; dx <= radius; dx++) {
                    int px = cx + dx, py = cy + dy;
                    if (px < 0 || px >= OUTPUT_WIDTH || py < 0 || py >= OUTPUT_HEIGHT) continue;
                    float r2 = (float)(dx * dx + dy * dy);
                    float val = brightness * expf(-r2 / (2.0f * sigma * sigma));
                    int idx = py * OUTPUT_WIDTH + px;
                    outR[idx] += val;
                    outG[idx] += val;
                    outB[idx] += val * 0.8f;
                }
            }
        }
        return write16BitRGBTiff(OUTPUT_WIDTH, OUTPUT_HEIGHT, outR, outG, outB);
    }

    // Normalize flux to [0,1] using 0.1%/99.9% percentile
    {
        std::vector<float> allFlux;
        allFlux.reserve(npix * 3);
        for (int i = 0; i < npix; i++) {
            if (srcR[i] != 0) allFlux.push_back(srcR[i]);
            if (srcG[i] != 0) allFlux.push_back(srcG[i]);
            if (srcB[i] != 0) allFlux.push_back(srcB[i]);
        }
        if (allFlux.empty()) {
            qWarning() << "All-zero FITS data (should not happen after synthetic fallback)";
            return QByteArray();
        }

        std::sort(allFlux.begin(), allFlux.end());
        float fluxMin = allFlux[(size_t)(allFlux.size() * 0.001)];
        float fluxMax = allFlux[(size_t)(allFlux.size() * 0.999)];
        if (fluxMax <= fluxMin) fluxMax = fluxMin + 1.0f;
        float scale = 1.0f / (fluxMax - fluxMin);

        qDebug() << "Flux range:" << fluxMin << "to" << fluxMax << "nanomaggies";

        for (int i = 0; i < npix; i++) {
            srcR[i] = std::max(0.0f, std::min(1.0f, (srcR[i] - fluxMin) * scale));
            srcG[i] = std::max(0.0f, std::min(1.0f, (srcG[i] - fluxMin) * scale));
            srcB[i] = std::max(0.0f, std::min(1.0f, (srcB[i] - fluxMin) * scale));
        }
    }

    // Compute crop offset from tile center to target position
    // FITS convention: x increases with RA (east), y increases with Dec (north)
    // Legacy Survey cutouts: CRPIX at center, CD matrix has negative CD1_1 (RA decreases with x)
    // So positive deltaRA = pixel to the LEFT (negative x offset)
    double deltaRA  = (target_ra - tile_center_ra) * cos(tile_center_dec * M_PI / 180.0);
    double deltaDec = target_dec - tile_center_dec;

    // Convert to pixels (negative sign on RA because RA increases leftward in standard orientation)
    int offsetX_from_center = (int)(-deltaRA * 3600.0 / PIXSCALE_ARCSEC);
    int offsetY_from_center = (int)(deltaDec * 3600.0 / PIXSCALE_ARCSEC);

    // Crop rectangle in tile coordinates (FITS: row 0 = bottom = south)
    int cropX = tileW / 2 + offsetX_from_center - OUTPUT_WIDTH / 2;
    int cropY = tileH / 2 + offsetY_from_center - OUTPUT_HEIGHT / 2;

    // Clamp to tile bounds
    cropX = std::max(0, std::min(cropX, tileW - OUTPUT_WIDTH));
    cropY = std::max(0, std::min(cropY, tileH - OUTPUT_HEIGHT));

    qDebug() << "Crop:" << cropX << cropY << "size" << OUTPUT_WIDTH << "x" << OUTPUT_HEIGHT;

    // Extract cropped region into output channels
    // Flip Y so row 0 = top (north) in output TIFF
    int outN = OUTPUT_WIDTH * OUTPUT_HEIGHT;
    std::vector<float> outR(outN), outG(outN), outB(outN);

    for (int oy = 0; oy < OUTPUT_HEIGHT; oy++) {
        int srcY = (cropY + OUTPUT_HEIGHT - 1 - oy);  // flip Y
        for (int ox = 0; ox < OUTPUT_WIDTH; ox++) {
            int srcIdx = srcY * tileW + (cropX + ox);
            int outIdx = oy * OUTPUT_WIDTH + ox;
            outR[outIdx] = srcR[srcIdx];
            outG[outIdx] = srcG[srcIdx];
            outB[outIdx] = srcB[srcIdx];
        }
    }

    return write16BitRGBTiff(OUTPUT_WIDTH, OUTPUT_HEIGHT, outR, outG, outB);
}

// ---------------------------------------------------------------------------
// 16-bit RGB TIFF writer (from normalized [0,1] float channels)
// ---------------------------------------------------------------------------

QByteArray DSSFitsManager::write16BitRGBTiff(int w, int h,
                                              const std::vector<float> &rCh,
                                              const std::vector<float> &gCh,
                                              const std::vector<float> &bCh)
{
    quint32 stripBytes = w * h * 3 * 2;
    quint32 pixelDataOffset = 8;
    quint32 ifdOffset = 8 + stripBytes;

    int numEntries = 11;
    quint32 ifdSize = 2 + numEntries * 12 + 4;
    quint32 bpsArrayOffset = ifdOffset + ifdSize;

    QByteArray tiff;
    tiff.reserve(8 + stripBytes + ifdSize + 6);

    auto put16 = [&](quint16 v) {
        tiff.append((char)(v & 0xFF));
        tiff.append((char)((v >> 8) & 0xFF));
    };
    auto put32 = [&](quint32 v) {
        tiff.append((char)(v & 0xFF));
        tiff.append((char)((v >> 8) & 0xFF));
        tiff.append((char)((v >> 16) & 0xFF));
        tiff.append((char)((v >> 24) & 0xFF));
    };

    // Header
    tiff.append("II", 2);
    put16(42);
    put32(ifdOffset);

    // Pixel data: interleaved 16-bit RGB
    for (int i = 0; i < w * h; i++) {
        put16((quint16)(std::max(0.0f, std::min(1.0f, rCh[i])) * 65535.0f));
        put16((quint16)(std::max(0.0f, std::min(1.0f, gCh[i])) * 65535.0f));
        put16((quint16)(std::max(0.0f, std::min(1.0f, bCh[i])) * 65535.0f));
    }

    // IFD
    put16(numEntries);

    auto ifdShort = [&](quint16 tag, quint16 val) {
        put16(tag); put16(3); put32(1); put16(val); put16(0);
    };
    auto ifdLong = [&](quint16 tag, quint32 val) {
        put16(tag); put16(4); put32(1); put32(val);
    };
    auto ifdShortPtr = [&](quint16 tag, quint32 count, quint32 offset) {
        put16(tag); put16(3); put32(count); put32(offset);
    };

    ifdShort(256, w);                        // ImageWidth
    ifdShort(257, h);                        // ImageLength
    ifdShortPtr(258, 3, bpsArrayOffset);     // BitsPerSample -> 3 x 16
    ifdShort(259, 1);                        // Compression = None
    ifdShort(262, 2);                        // PhotometricInterpretation = RGB
    ifdLong(273, pixelDataOffset);           // StripOffsets
    ifdShort(274, 1);                        // Orientation = TopLeft
    ifdShort(277, 3);                        // SamplesPerPixel = 3
    ifdLong(278, h);                         // RowsPerStrip = all rows
    ifdLong(279, stripBytes);                // StripByteCounts
    ifdShort(284, 1);                        // PlanarConfiguration = Chunky

    put32(0);  // Next IFD = none

    // BitsPerSample array
    put16(16); put16(16); put16(16);

    return tiff;
}

// ---------------------------------------------------------------------------
// Cache management
// ---------------------------------------------------------------------------

CachedFitsImage* DSSFitsManager::findCachedTileContaining(double ra_deg, double dec_deg)
{
    double output_w_arcmin = OUTPUT_WIDTH  * PIXSCALE_ARCSEC / 60.0;
    double output_h_arcmin = OUTPUT_HEIGHT * PIXSCALE_ARCSEC / 60.0;

    for (CachedFitsImage &cached : m_cachedImages) {
        if (cached.containsPosition(ra_deg, dec_deg, output_w_arcmin, output_h_arcmin))
            return &cached;
    }
    return nullptr;
}

void DSSFitsManager::addToCacheIndex(const CachedFitsImage &image)
{
    m_cachedImages.append(image);
    saveCacheIndex();
}

void DSSFitsManager::loadCacheIndex()
{
    m_cachedImages.clear();

    int size = m_cacheIndex->beginReadArray("cached_images");
    for (int i = 0; i < size; ++i) {
        m_cacheIndex->setArrayIndex(i);

        CachedFitsImage cached;
        cached.cacheKey       = m_cacheIndex->value("key").toString();
        cached.center_ra_deg  = m_cacheIndex->value("ra").toDouble();
        cached.center_dec_deg = m_cacheIndex->value("dec").toDouble();
        cached.width_arcmin   = m_cacheIndex->value("width").toDouble();
        cached.height_arcmin  = m_cacheIndex->value("height").toDouble();
        cached.fitsFilePath   = m_cacheIndex->value("fits_path").toString();
        cached.fetchTime      = m_cacheIndex->value("fetch_time").toDateTime();

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

void DSSFitsManager::saveCacheIndex()
{
    m_cacheIndex->beginWriteArray("cached_images");
    for (int i = 0; i < m_cachedImages.size(); ++i) {
        m_cacheIndex->setArrayIndex(i);
        const CachedFitsImage &c = m_cachedImages[i];

        m_cacheIndex->setValue("key", c.cacheKey);
        m_cacheIndex->setValue("ra", c.center_ra_deg);
        m_cacheIndex->setValue("dec", c.center_dec_deg);
        m_cacheIndex->setValue("width", c.width_arcmin);
        m_cacheIndex->setValue("height", c.height_arcmin);
        m_cacheIndex->setValue("fits_path", c.fitsFilePath);
        m_cacheIndex->setValue("fetch_time", c.fetchTime);
    }
    m_cacheIndex->endArray();
    m_cacheIndex->sync();
}

void DSSFitsManager::clearCache()
{
    for (const CachedFitsImage &c : m_cachedImages)
        QFile::remove(c.fitsFilePath);
    m_cachedImages.clear();
    saveCacheIndex();
    qDebug() << "Cache cleared";
}

qint64 DSSFitsManager::getCacheSize() const
{
    qint64 size = 0;
    for (const CachedFitsImage &c : m_cachedImages) {
        QFileInfo fi(c.fitsFilePath);
        size += fi.size();
    }
    return size;
}
