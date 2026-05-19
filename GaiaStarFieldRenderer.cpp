#include "GaiaStarFieldRenderer.h"
#include "StellariumDSOOverlay.h"

#include <pcl/GaiaDatabaseFile.h>

#include <QDir>
#include <QDebug>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

#include <cmath>
#include <algorithm>
#include <numeric>
#include <random>

using namespace pcl;

// ---------------------------------------------------------------------------
// Constructor — probe for Gaia XPSD database files
// ---------------------------------------------------------------------------

GaiaStarFieldRenderer::GaiaStarFieldRenderer(QObject *parent)
    : QObject(parent)
{
    m_dbDir = QDir::homePath() + "/PixInsight/databases";
    QDir dir(m_dbDir);
    QStringList files = dir.entryList(QStringList() << "gdr3-*.xpsd", QDir::Files);
    if (files.isEmpty())
        files = dir.entryList(QStringList() << "gdr3sp-*.xpsd" << "gedr3-*.xpsd", QDir::Files);

    m_available = !files.isEmpty();
    if (m_available)
        qDebug() << "GaiaStarFieldRenderer: using" << files.size()
                 << "XPSD database files from" << m_dbDir
                 << "(" << files.first() << "...)";
    else
        qDebug() << "GaiaStarFieldRenderer: no Gaia databases found in" << m_dbDir;
}

// ---------------------------------------------------------------------------
// BP-RP color index to approximate visual RGB
// ---------------------------------------------------------------------------

void GaiaStarFieldRenderer::bpRpToRGB(float bp_rp, float &rr, float &gg, float &bb)
{
    float t = std::max(-0.5f, std::min(4.0f, bp_rp));

    if (t < 0.0f) {
        rr = 0.6f + 0.4f * (t + 0.5f) / 0.5f;
        gg = 0.7f + 0.3f * (t + 0.5f) / 0.5f;
        bb = 1.0f;
    } else if (t < 0.5f) {
        rr = 1.0f;
        gg = 1.0f;
        bb = 1.0f - 0.2f * t / 0.5f;
    } else if (t < 1.0f) {
        float u = (t - 0.5f) / 0.5f;
        rr = 1.0f;
        gg = 1.0f - 0.15f * u;
        bb = 0.8f - 0.3f * u;
    } else if (t < 2.0f) {
        float u = (t - 1.0f) / 1.0f;
        rr = 1.0f;
        gg = 0.85f - 0.35f * u;
        bb = 0.5f - 0.3f * u;
    } else {
        float u = std::min(1.0f, (t - 2.0f) / 2.0f);
        rr = 1.0f;
        gg = 0.5f - 0.3f * u;
        bb = 0.2f - 0.15f * u;
    }
}

// ---------------------------------------------------------------------------
// 16-bit RGB TIFF writer
// ---------------------------------------------------------------------------

QByteArray GaiaStarFieldRenderer::write16BitRGBTiff(
    int w, int h,
    const std::vector<uint16_t> &rCh,
    const std::vector<uint16_t> &gCh,
    const std::vector<uint16_t> &bCh,
    const TiffMetadata* meta)
{
    // ============================================================
    // Build the optional auxiliary data first (we need their sizes
    // before we can fix the IFD entry offsets).
    // ============================================================
    const QByteArray softwareTag = QByteArray("Origin 1.2.5227 08-15-2025 17:51") + '\0';

    // Decompose a radians-as-decimal-degrees angle into (deg, min, sec) for
    // the GPS lat/long rationals.
    auto degMinSec = [](double rad, int &deg, int &min, double &sec) {
        double absDeg = std::fabs(rad) * 180.0 / M_PI;
        deg = int(std::floor(absDeg));
        double rem = (absDeg - deg) * 60.0;
        min = int(std::floor(rem));
        sec = (rem - min) * 60.0;
    };

    // Compose the JSON MakerNote payload from the metadata.
    QByteArray makerNote;
    QByteArray dateTimeOriginal;   // "YYYY:MM:DD HH:MM:SS" — 20 bytes inc null
    QByteArray offsetTimeOriginal; // "+HH:MM" — 7 bytes inc null
    QByteArray uuidStr;
    QByteArray objectStr;
    QByteArray filterStr;
    QByteArray bayerStr;
    if (meta) {
        QDateTime now = QDateTime::currentDateTime();
        // Format the timezone offset as numeric "+HHMM" — Qt's "t" token
        // returns the TZ abbreviation ("BST"/"GMT"/...) rather than a numeric
        // offset, which the App may not accept. Match the real telescope:
        // "2025-05-19T22:22:12+0100".
        auto numericOffset = [](const QDateTime& dt) -> QString {
            int s = dt.offsetFromUtc();
            const char sign = (s >= 0 ? '+' : '-');
            s = std::abs(s);
            return QString("%1%2%3").arg(sign)
                                    .arg(s / 3600, 2, 10, QChar('0'))
                                    .arg((s % 3600) / 60, 2, 10, QChar('0'));
        };
        QString iso = meta->isoDateTime;
        if (iso.isEmpty())
            iso = now.toString("yyyy-MM-ddTHH:mm:ss") + numericOffset(now);
        QString uuidQ = meta->uuid;
        if (uuidQ.isEmpty())
            uuidQ = QUuid::createUuid().toString(QUuid::WithoutBraces).toUpper();
        uuidStr   = uuidQ.toUtf8();
        objectStr = meta->objectName.toUtf8();
        filterStr = meta->filter.toUtf8();
        bayerStr  = meta->bayer.toUtf8();

        const double totalDurationMs = double(meta->stackDepth) * meta->exposureSec * 1000.0;

        QJsonObject stackedInfo;
        stackedInfo["bayer"] = meta->bayer;
        QJsonObject captureParams;
        captureParams["autoExposure"] = true;
        captureParams["binning"]      = 1;
        captureParams["exposure"]     = meta->exposureSec;
        captureParams["iso"]          = meta->iso;
        captureParams["temperature"]  = meta->cameraTempC;
        stackedInfo["captureParams"] = captureParams;
        QJsonObject celestial;
        celestial["first"]  = meta->raRad;
        celestial["second"] = meta->decRad;
        stackedInfo["celestial"]   = celestial;
        stackedInfo["dateTime"]    = iso;
        stackedInfo["filter"]      = meta->filter;
        stackedInfo["fovX"]        = meta->fovXRad;
        stackedInfo["fovY"]        = meta->fovYRad;
        QJsonObject gps;
        gps["altitude"]  = meta->altitudeM;
        gps["latitude"]  = meta->latRad * 180.0 / M_PI;
        gps["longitude"] = meta->lonRad * 180.0 / M_PI;
        stackedInfo["gps"] = gps;
        stackedInfo["imageHeight"]       = meta->imageHeight;
        stackedInfo["imageWidth"]        = meta->imageWidth;
        stackedInfo["objectName"]        = meta->objectName;
        stackedInfo["orientation"]       = meta->orientationRad;
        stackedInfo["stackedDepth"]      = meta->stackDepth;
        stackedInfo["stretchBackground"] = meta->stretchBackground;
        stackedInfo["stretchStrength"]   = meta->stretchStrength;
        stackedInfo["totalDurationMs"]   = totalDurationMs;
        stackedInfo["uuid"]              = uuidQ;
        QJsonObject root;
        root["StackedInfo"] = stackedInfo;
        makerNote = QJsonDocument(root).toJson(QJsonDocument::Compact);
        // Real telescope's MakerNote isn't null-terminated; keep raw bytes.

        // Exif DateTimeOriginal + OffsetTimeOriginal use the same now timestamp.
        const QString dtStr  = now.toString("yyyy:MM:dd HH:mm:ss");
        const QString offNum = numericOffset(now);  // "+0100"
        dateTimeOriginal = (dtStr + '\0').toLatin1();
        // Convert "+0100" → "+01:00" for OffsetTimeOriginal (Exif spec).
        const QString offWithColon = offNum.left(3) + ':' + offNum.right(2);
        offsetTimeOriginal = (offWithColon + '\0').toLatin1();
    }
    const bool hasMeta = (meta != nullptr);

    // ============================================================
    // Layout computation (offsets are file-relative).
    // ============================================================
    const quint32 stripBytes      = quint32(w) * quint32(h) * 3u * 2u;
    const quint32 pixelDataOffset = 8;
    const quint32 ifdOffset       = pixelDataOffset + stripBytes;
    const int     numMainEntries  = hasMeta ? 15 : 12;
    const quint32 mainIfdSize     = 2u + quint32(numMainEntries) * 12u + 4u;

    // Place trailing data after the main IFD:
    quint32 cursor = ifdOffset + mainIfdSize;
    const quint32 bpsArrayOffset  = cursor; cursor += 6;
    const quint32 softwareOffset  = cursor; cursor += quint32(softwareTag.size());

    quint32 gpsDataOffset = 0, gpsIfdOffset = 0;
    quint32 dateTimeOffset = 0, offsetTimeOffset = 0, exposureRatOffset = 0;
    quint32 makerNoteOffset = 0, exifIfdOffset = 0;
    if (hasMeta) {
        // GPS rational data: lat (3×8 bytes), lon (3×8 bytes), alt (8 bytes) = 56 bytes
        gpsDataOffset = cursor; cursor += 8u * 3 + 8u * 3 + 8u;
        // GPS IFD: numEntries(2) + 7×12 + nextIFD(4) = 90 bytes
        gpsIfdOffset  = cursor; cursor += 90;
        // Exif ASCII / rational / undefined data, in order
        dateTimeOffset     = cursor; cursor += quint32(dateTimeOriginal.size());
        offsetTimeOffset   = cursor; cursor += quint32(offsetTimeOriginal.size());
        exposureRatOffset  = cursor; cursor += 8;  // ExposureTime rational
        makerNoteOffset    = cursor; cursor += quint32(makerNote.size());
        // Exif IFD: numEntries(2) + 7×12 + nextIFD(4) = 90 bytes
        exifIfdOffset      = cursor; cursor += 90;
    }
    const quint32 totalSize = cursor;

    QByteArray tiff;
    tiff.reserve(totalSize);

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

    // ---- Header (8 bytes) ----
    tiff.append("II", 2);
    put16(42);
    put32(ifdOffset);

    // ---- Pixel data ----
    {
        int npix = w * h;
        QByteArray pixelData(npix * 3 * 2, Qt::Uninitialized);
        quint16 *dst = reinterpret_cast<quint16*>(pixelData.data());
        for (int i = 0; i < npix; i++) {
            dst[i * 3 + 0] = rCh[i];
            dst[i * 3 + 1] = gCh[i];
            dst[i * 3 + 2] = bCh[i];
        }
        tiff.append(pixelData);
    }

    // ---- Main IFD ----
    put16(quint16(numMainEntries));

    auto ifdShort = [&](quint16 tag, quint16 val) {
        put16(tag); put16(3); put32(1); put16(val); put16(0);
    };
    auto ifdLong = [&](quint16 tag, quint32 val) {
        put16(tag); put16(4); put32(1); put32(val);
    };
    auto ifdShortPtr = [&](quint16 tag, quint32 count, quint32 offset) {
        put16(tag); put16(3); put32(count); put32(offset);
    };
    auto ifdAsciiPtr = [&](quint16 tag, quint32 count, quint32 offset) {
        put16(tag); put16(2); put32(count); put32(offset);
    };
    auto ifdIfdPtr = [&](quint16 tag, quint32 offset) {
        put16(tag); put16(13); put32(1); put32(offset);
    };

    ifdShort(256, w);
    ifdShort(257, h);
    ifdShortPtr(258, 3, bpsArrayOffset);
    ifdShort(259, 1);
    ifdShort(262, 2);
    ifdLong(273, pixelDataOffset);
    ifdShort(274, 1);
    ifdShort(277, 3);
    ifdLong(278, h);
    ifdLong(279, stripBytes);
    ifdShort(284, 1);
    ifdAsciiPtr(305, quint32(softwareTag.size()), softwareOffset);
    if (hasMeta) {
        ifdIfdPtr(34665, exifIfdOffset);   // Exif IFD pointer
        ifdIfdPtr(34853, gpsIfdOffset);    // GPS IFD pointer
        ifdShort(50741, 1);                // MakerNoteSafety = 1
    }
    put32(0);                              // next IFD = none

    // ---- BPS array ----
    put16(16); put16(16); put16(16);

    // ---- Software string ----
    tiff.append(softwareTag);

    if (hasMeta) {
        // ---- GPS rational data ----
        auto putRat = [&](quint32 num, quint32 den) { put32(num); put32(den); };
        // Latitude DMS
        int lat_d, lat_m; double lat_s;
        degMinSec(meta->latRad, lat_d, lat_m, lat_s);
        putRat(quint32(lat_d), 1);
        putRat(quint32(lat_m), 1);
        // Seconds expressed with high precision: encode as integer micro-arcseconds
        putRat(quint32(lat_s * 1e6), 1000000);
        // Longitude DMS
        int lon_d, lon_m; double lon_s;
        degMinSec(meta->lonRad, lon_d, lon_m, lon_s);
        putRat(quint32(lon_d), 1);
        putRat(quint32(lon_m), 1);
        putRat(quint32(lon_s * 1e6), 1000000);
        // Altitude rational
        putRat(quint32(std::max(0.0, meta->altitudeM)), 1);

        // ---- GPS IFD ----
        put16(7);                                              // numEntries
        // 0: GPSVersionID (BYTE × 4) = 2.3.0.0
        put16(0); put16(1); put32(4);
        tiff.append((char)2); tiff.append((char)3);
        tiff.append((char)0); tiff.append((char)0);
        // 1: GPSLatitudeRef (ASCII 2) "N\0" or "S\0"
        put16(1); put16(2); put32(2);
        tiff.append(meta->latRad >= 0 ? 'N' : 'S');
        tiff.append('\0'); tiff.append('\0'); tiff.append('\0');
        // 2: GPSLatitude (RATIONAL × 3) offset
        put16(2); put16(5); put32(3); put32(gpsDataOffset);
        // 3: GPSLongitudeRef
        put16(3); put16(2); put32(2);
        tiff.append(meta->lonRad >= 0 ? 'E' : 'W');
        tiff.append('\0'); tiff.append('\0'); tiff.append('\0');
        // 4: GPSLongitude (RATIONAL × 3) offset
        put16(4); put16(5); put32(3); put32(gpsDataOffset + 24);
        // 5: GPSAltitudeRef (BYTE = 0)
        put16(5); put16(1); put32(1); put32(0);
        // 6: GPSAltitude (RATIONAL × 1) offset
        put16(6); put16(5); put32(1); put32(gpsDataOffset + 48);
        put32(0);                                              // next IFD = none

        // ---- Exif ASCII / rational / undefined data ----
        tiff.append(dateTimeOriginal);
        tiff.append(offsetTimeOriginal);
        // ExposureTime rational: num=exposure_x1000, den=1000 → preserves decimal seconds
        const quint32 expNum = quint32(meta->exposureSec * 1000.0);
        put32(expNum); put32(1000);
        // MakerNote raw bytes (no null terminator)
        tiff.append(makerNote);

        // ---- Exif IFD ----
        put16(7);                                              // numEntries
        // 33434 ExposureTime (RATIONAL × 1)
        put16(33434); put16(5); put32(1); put32(exposureRatOffset);
        // 34867 ISOSpeed (LONG × 1) — inline
        put16(34867); put16(4); put32(1); put32(quint32(meta->iso));
        // 36864 ExifVersion (UNDEFINED × 4) = "0231"
        put16(36864); put16(7); put32(4);
        tiff.append('0'); tiff.append('2'); tiff.append('3'); tiff.append('1');
        // 36867 DateTimeOriginal (ASCII × len) — pointer
        put16(36867); put16(2); put32(quint32(dateTimeOriginal.size())); put32(dateTimeOffset);
        // 36881 OffsetTimeOriginal (ASCII × len) — pointer
        put16(36881); put16(2); put32(quint32(offsetTimeOriginal.size())); put32(offsetTimeOffset);
        // 37500 MakerNote (UNDEFINED × len) — pointer
        put16(37500); put16(7); put32(quint32(makerNote.size())); put32(makerNoteOffset);
        // 41986 ExposureMode (SHORT × 1) = 0 (Auto)
        put16(41986); put16(3); put32(1); put16(0); put16(0);
        put32(0);                                              // next IFD = none
    }

    return tiff;
}

// ---------------------------------------------------------------------------
// Main render entry point
// ---------------------------------------------------------------------------

QByteArray GaiaStarFieldRenderer::renderField(double ra_deg, double dec_deg,
                                               int width, int height,
                                               double pixscale_arcsec,
                                               const SkyConditions& sky,
                                               double cameraRotationDeg,
                                               double exposureRotationDeg,
                                               const StellariumDSOOverlay* dsoOverlay,
                                               int    stackDepth,
                                               const TiffMetadata* tiffMeta)
{
    if (!m_available) {
        qWarning() << "GaiaStarFieldRenderer: no database available";
        return QByteArray();
    }

    double resolution = pixscale_arcsec / 3600.0;   // degrees/pixel
    double fovW = width  * resolution;
    double fovH = height * resolution;
    double fieldRadius = std::sqrt(fovW * fovW + fovH * fovH) / 2.0 * 1.05;

    qDebug() << QString("Gaia render: RA=%1, Dec=%2, FOV=%3x%4 deg, pixscale=%5\"/px")
                .arg(ra_deg, 0, 'f', 4).arg(dec_deg, 0, 'f', 4)
                .arg(fovW, 0, 'f', 2).arg(fovH, 0, 'f', 2)
                .arg(pixscale_arcsec, 0, 'f', 2);

    // Magnitude limit — go deeper to ensure enough stars for plate solving.
    // For the Origin's ~2° FOV the adaptive formula gives ~12.6, but gdr3sp
    // databases are sparse so we always go to at least mag 16.
    float limitMag = std::max(16.0f,
        float(14.5 * std::pow(std::max(fovW, fovH), -0.179)));

    // Query database files — prefer full gdr3 over gdr3sp (spectrum subset)
    QDir dir(m_dbDir);
    QStringList dbFiles = dir.entryList(QStringList() << "gdr3-*.xpsd", QDir::Files, QDir::Name);
    if (dbFiles.isEmpty())
        dbFiles = dir.entryList(QStringList() << "gdr3sp-*.xpsd" << "gedr3-*.xpsd", QDir::Files, QDir::Name);

    std::vector<GaiaStarData> allStars;

    for (const QString &dbFile : dbFiles) {
        QString fullPath = dir.absoluteFilePath(dbFile);
        try {
            GaiaDatabaseFile gaiaDB(String::UTF8ToUTF16(fullPath.toUtf8().constData()));

            GaiaSearchData search;
            search.centerRA = ra_deg;
            search.centerDec = dec_deg;
            search.radius = fieldRadius;
            search.magnitudeLow = -1.5;
            search.magnitudeHigh = limitMag;
            search.sourceLimit = 50000;
            gaiaDB.Search(search);

            for (size_type i = 0; i < search.stars.Length(); ++i)
                allStars.push_back(search.stars[i]);

            qDebug() << "  " << dbFile << ":" << search.stars.Length() << "stars";
        } catch (const pcl::Exception &e) {
            qWarning() << "  Error opening" << dbFile
                       << ":" << QString::fromUtf8(e.Message().ToUTF8().c_str());
        } catch (...) {
            qWarning() << "  Unknown error opening" << dbFile;
        }
    }

    qDebug() << "Total Gaia stars:" << allStars.size() << "(mag limit" << limitMag << ")";

    if (allStars.empty()) {
        qWarning() << "No Gaia stars found in this region";
        return QByteArray();
    }

    // Build a sorted index by brightness — avoid sorting GaiaStarData directly
    // since its FVector member uses PCL reference counting that doesn't survive std::sort.
    std::vector<size_t> starIdx(allStars.size());
    std::iota(starIdx.begin(), starIdx.end(), 0);
    std::sort(starIdx.begin(), starIdx.end(),
              [&](size_t a, size_t b) { return allStars[a].magG < allStars[b].magG; });
    // Cap to brightest 500. Real Origin live previews show ~30-60 detectable
    // stars per field; capping at 500 leaves plenty for plate-solving triangle
    // matching without flooding the App's star detector with sources at the
    // noise floor (which causes "Can't see stars" — too many false positives).
    const size_t maxStars = 500;
    if (starIdx.size() > maxStars) {
        qDebug() << "Capping render to" << maxStars << "brightest stars (was" << starIdx.size() << ")";
        starIdx.resize(maxStars);
    }

    // --- Render ---
    // Brightness model calibrated for the Origin telescope:
    // 14cm f/2, 1.4777"/pixel (0.00041047 deg/px), Sony IMX571, 5s @ ISO 2000.
    // The f/2 is extremely fast — stars ≤ mag ~11 saturate.
    // refBrightness is the *unclamped* peak for the reference magnitude;
    // the 65535 clamp handles saturation automatically.
    //   mag  8: ~500k → clamped 65535  (saturated)
    //   mag 10: ~79k  → clamped 65535  (saturated)
    //   mag 11: ~31k                   — very bright
    //   mag 12: ~12600                 — bright
    //   mag 14:  ~2000                 — clearly visible (4× sky)
    //   mag 15:   ~790                 — visible (1.6× sky)
    //   mag 16:   ~320                 — near sky level

    // Sensor baseline (bias + amp glow + dark current) measured from a real
    // Origin 10s sub: clean-region medians per Bayer channel ~4700/5300/4700
    // ADU. Dominant over Bortle-driven sky photons in this regime — see the
    // Bortle table below for the photon contribution that's added on top.
    const float skyR0 = 4700.0f;
    const float skyG0 = 5300.0f;
    const float skyB0 = 4700.0f;
    // Read noise σ in ADU. The 270 ADU figure measured from real FITS sky
    // regions includes Bayer-pattern leakage, hot pixels, and 1/f / amp-glow
    // structure that real detectors subtract spatially — not pure gaussian.
    // Adding 270 ADU of gaussian to every pixel drowns the faint stars the
    // App's detector relies on. 50 ADU captures the genuine read-noise
    // component without overwhelming detection.
    const float readNoise     = 50.0f;
    const float refBrightness = 500000.0f; // unclamped peak ADU for a mag 8 star
    const float refMag        = 8.0f;

    // Bortle-class sky photons. Zenith night-sky brightness vs class:
    //   1: 22.0  2: 21.9  3: 21.7  4: 21.3  5: 20.5  6: 19.7  7: 19.0
    //   8: 18.5  9: 17.5  (V mag/arcsec²)
    // Per-pixel/per-second photon flux at Bortle 5 ≈ 0.7 ADU at unit gain on
    // the Origin (14cm aperture, QE 0.7, 1.477"/px, broadband÷3 for Bayer).
    // Other classes scale by 10^(0.4·(20.5 − μ)).
    static constexpr float bortleNSB[10] = {
        // index 0 unused; use 1..9
        0.0f, 22.0f, 21.9f, 21.7f, 21.3f, 20.5f, 19.7f, 19.0f, 18.5f, 17.5f
    };
    const int b = std::clamp(sky.bortleClass, 1, 9);
    const float bortleSkyADU = 0.7f * float(sky.exposureSec)
                             * std::pow(10.0f, 0.4f * (20.5f - bortleNSB[b]));

    // Rayleigh: sky brightens with airmass; blue scatters more than red.
    // Disabled by default — the airmass amplification can wash out the App's
    // star detector at moderate altitudes. Coefficients are tuned for plausible
    // night-sky brightening when enabled.
    float rayleighR = 1.0f, rayleighG = 1.0f, rayleighB = 1.0f;
    if (sky.rayleighEnabled) {
        double pointingAlt = sky.altitudeDeg;
        if (pointingAlt < 2.0)  pointingAlt = 2.0;   // clamp; sec(z) blows up at horizon
        if (pointingAlt > 90.0) pointingAlt = 90.0;
        const double airmass = std::min(1.0 / std::cos((90.0 - pointingAlt) * M_PI / 180.0), 38.0);
        rayleighR = float(1.0 + (airmass - 1.0) * 0.05);
        rayleighG = float(1.0 + (airmass - 1.0) * 0.10);
        rayleighB = float(1.0 + (airmass - 1.0) * 0.25);
    }

    // Moonglow: broad halo around the Moon's position, white-blue. Drops off
    // as a Gaussian with σ ≈ 15° (so contribution is negligible by ~45°). Only
    // contributes when the Moon is above the horizon. Scales linearly with
    // illuminated fraction.
    float moonR = 0.0f, moonG = 0.0f, moonB = 0.0f;
    if (sky.moonUp) {
        const float sigmaDeg = 15.0f;
        const float arg = float(sky.moonSepDeg) / sigmaDeg;
        const float falloff = std::exp(-0.5f * arg * arg);
        const float intensity = float(sky.moonPhase) * falloff;
        moonR = 200.0f * intensity;
        moonG = 250.0f * intensity;
        moonB = 300.0f * intensity;
    }

    const float skyR = skyR0 * rayleighR + moonR + bortleSkyADU;
    const float skyG = skyG0 * rayleighG + moonG + bortleSkyADU;
    const float skyB = skyB0 * rayleighB + moonB + bortleSkyADU;

    int npix = width * height;
    std::vector<float> imgR(npix, skyR);
    std::vector<float> imgG(npix, skyG);
    std::vector<float> imgB(npix, skyB);

    // Add Gaussian read noise — independent per frame and per channel. The
    // earlier version seeded with (ra,dec) so a stationary pointing gave the
    // bit-identical noise field every frame, and used the same scalar noise
    // value across R/G/B per pixel. Both broke stacking realism: real CMOS
    // noise is independent across frames and channels (√N improvement on
    // sky background, no fixed-pattern leakage when subs are co-added).
    static thread_local std::mt19937 noiseRng{ std::random_device{}() };
    std::normal_distribution<float> noiseDist(0.0f, readNoise);
    for (int i = 0; i < npix; i++) {
        imgR[i] += noiseDist(noiseRng);
        imgG[i] += noiseDist(noiseRng);
        imgB[i] += noiseDist(noiseRng);
    }

    // Gnomonic (TAN) projection. The previous small-angle approximation
    // (dRA *= cos(dec_center)) collapses all stars onto a single column when
    // dec_center is at the celestial pole, because cos(±90°)=0. The full TAN
    // projection handles arbitrary pointings including the pole.
    constexpr double deg2rad = M_PI / 180.0;
    constexpr double rad2deg = 180.0 / M_PI;
    const double dec0Rad = dec_deg * deg2rad;
    const double sinDec0 = std::sin(dec0Rad);
    const double cosDec0 = std::cos(dec0Rad);

    // Field rotation: rotate the tangent-plane coords around the pointing
    // centre. An alt-az mount cannot compensate for Earth's rotation about
    // the celestial axis, so the camera frame slowly turns relative to the
    // sky over the course of an imaging session.
    const double rotRad = cameraRotationDeg * deg2rad;
    const double cosRot = std::cos(rotRad);
    const double sinRot = std::sin(rotRad);

    // Exposure-time trailing. During a long exposure on an alt-az mount the
    // field rotates while the shutter is open; stars far from the pointing
    // centre trace arcs. Subsample the PSF along the arc, with the count
    // chosen so the worst-case (corner star) step is ≤ 0.5 px. Capped to
    // keep render time bounded near zenith where rotation rate diverges.
    const double maxR_px      = 0.5 * std::hypot(double(width), double(height));
    const double maxTrail_px  = maxR_px * std::fabs(exposureRotationDeg) * deg2rad;
    const int    trailSubN    = std::clamp(int(std::ceil(maxTrail_px / 0.5)), 1, 100);
    const float  trailFluxInv = 1.0f / float(trailSubN);

    for (size_t si : starIdx) {
        const auto &star = allStars[si];
        double dRA_deg = star.ra - ra_deg;
        if (dRA_deg > 180)  dRA_deg -= 360;
        if (dRA_deg < -180) dRA_deg += 360;

        const double dRA    = dRA_deg * deg2rad;
        const double decRad = star.dec * deg2rad;
        const double sinDec = std::sin(decRad);
        const double cosDec = std::cos(decRad);
        const double sinDRA = std::sin(dRA);
        const double cosDRA = std::cos(dRA);

        // Direction-cosine denominator. <=0 means the star is on the far
        // hemisphere relative to the tangent point — skip.
        const double denom = sinDec0 * sinDec + cosDec0 * cosDec * cosDRA;
        if (denom <= 0.0) continue;

        const double L = cosDec * sinDRA / denom;
        const double M = (cosDec0 * sinDec - sinDec0 * cosDec * cosDRA) / denom;

        const double L_deg = L * rad2deg;
        const double M_deg = M * rad2deg;

        // Rotate around the pointing centre by cameraRotationDeg. Defer the
        // pixel conversion — we may rotate further by sub-exposure phases to
        // trail the PSF along an arc.
        const double Lr = L_deg * cosRot - M_deg * sinRot;
        const double Mr = L_deg * sinRot + M_deg * cosRot;

        // Brightness: peak ADU from magnitude relative to reference.
        // Clamp to sensor full-well capacity — in a real CCD, charge doesn't
        // spread beyond the well depth, so very bright stars just saturate
        // at the core rather than bloating outward.
        float peakADU = refBrightness * std::pow(10.0f, -0.4f * (star.magG - refMag));
        peakADU = std::min(peakADU, 65000.0f);

        // Color from BP-RP
        float bp_rp = star.magBP - star.magRP;
        float cr, cg, cb;
        bpRpToRGB(bp_rp, cr, cg, cb);

        // Gaussian PSF — measured FWHM in real Origin 10s subs is ~6 raw px
        // (in single-channel space, then ~12 raw px in Bayer-aliased space).
        // sigma 2.5 px gives FWHM ≈ 5.9 px ≈ 9″, matching well-focused frames
        // without going to the heavy-defocus 18″ extreme of the reference set.
        const float sigma = 2.5f;
        const int   psfRadius = (int)(sigma * 5);
        const float perSampleFlux = peakADU * trailFluxInv;

        for (int s = 0; s < trailSubN; ++s) {
            // Sub-exposure rotation phase, symmetric around 0 so the streak
            // is centred on the rendered pointing position.
            const double frac     = (trailSubN > 1) ? (double(s) / double(trailSubN - 1) - 0.5) : 0.0;
            const double thetaSub = frac * exposureRotationDeg * deg2rad;
            const double cosTs    = std::cos(thetaSub);
            const double sinTs    = std::sin(thetaSub);
            const double Ls = Lr * cosTs - Mr * sinTs;
            const double Ms = Lr * sinTs + Mr * cosTs;

            const double px = width  / 2.0 - Ls / resolution;
            const double py = height / 2.0 - Ms / resolution;
            const int ix = (int)std::round(px);
            const int iy = (int)std::round(py);
            if (ix < -20 || ix >= width + 20 || iy < -20 || iy >= height + 20)
                continue;

            for (int dy = -psfRadius; dy <= psfRadius; dy++) {
                for (int dx = -psfRadius; dx <= psfRadius; dx++) {
                    int x = ix + dx, y = iy + dy;
                    if (x < 0 || x >= width || y < 0 || y >= height) continue;
                    float r2 = (float)(dx * dx + dy * dy);
                    float val = perSampleFlux * std::exp(-r2 / (2.0f * sigma * sigma));
                    int idx = y * width + x;
                    imgR[idx] += val * cr;
                    imgG[idx] += val * cg;
                    imgB[idx] += val * cb;
                }
            }
        }
    }

    // Paint Stellarium nebulae/galaxies/clusters into the same float buffers
    // so they go through the same sky/stretch pipeline as the stars.
    if (dsoOverlay != nullptr) {
        dsoOverlay->paintInto(imgR, imgG, imgB, width, height,
                              resolution, ra_deg, dec_deg, cameraRotationDeg,
                              stackDepth);
    }

    // Convert to 16-bit, clamping to [0, 65535]
    std::vector<uint16_t> outR(npix), outG(npix), outB(npix);
    for (int i = 0; i < npix; i++) {
        outR[i] = (uint16_t)std::min(65535.0f, std::max(0.0f, imgR[i]));
        outG[i] = (uint16_t)std::min(65535.0f, std::max(0.0f, imgG[i]));
        outB[i] = (uint16_t)std::min(65535.0f, std::max(0.0f, imgB[i]));
    }

    QByteArray tiff = write16BitRGBTiff(width, height, outR, outG, outB, tiffMeta);
    qDebug() << "Gaia star field rendered:" << tiff.size() << "bytes,"
             << starIdx.size() << "of" << allStars.size() << "stars";
    return tiff;
}
