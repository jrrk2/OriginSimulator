#include "CelestronOriginSimulator.h"
#include <QApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QNetworkInterface>
#include <QDir>
#include <QFile>
#include <QBuffer>
#include <QImage>
#include <QDebug>
#include <QPainter>
#include <algorithm>
#include <cmath>
#include <random>

namespace {
// PixInsight-style Midtone Transfer Function. m is the midtone balance in (0,1):
// values < 0.5 brighten the midtones (stretch faint signal).
inline float mtf(float x, float m) {
    if (x <= 0.0f) return 0.0f;
    if (x >= 1.0f) return 1.0f;
    return (m - 1.0f) * x / ((2.0f * m - 1.0f) * x - m);
}

// Standard-normal random number (mean 0, σ 1). Thread-local RNG so renders
// from any thread stay reproducible per-thread.
double gaussian() {
    static thread_local std::mt19937 rng{ std::random_device{}() };
    static thread_local std::normal_distribution<double> dist{ 0.0, 1.0 };
    return dist(rng);
}

// Approximate Moon geocentric RA/Dec (radians) for Julian Date jd. Meeus
// low-precision — leading terms only, ~0.1° accurate. Plenty for moonglow
// simulation, where the halo is degrees wide.
void approximateMoonRaDec(double jd, double& raRad, double& decRad) {
    const double T = (jd - 2451545.0) / 36525.0;
    const double L = std::fmod(218.316 + 481267.881 * T, 360.0); // mean longitude
    const double M = std::fmod(134.963 + 477198.867 * T, 360.0); // mean anomaly
    const double F = std::fmod(93.272  + 483202.018 * T, 360.0); // arg of latitude

    const double lambdaRad = (L + 6.289 * std::sin(M * M_PI / 180.0)) * M_PI / 180.0;
    const double betaRad   = (5.128 * std::sin(F * M_PI / 180.0)) * M_PI / 180.0;
    const double eps       = (23.439 - 0.013 * T) * M_PI / 180.0;

    const double sinB = std::sin(betaRad),  cosB = std::cos(betaRad);
    const double sinL = std::sin(lambdaRad), cosL = std::cos(lambdaRad);
    const double sinE = std::sin(eps),       cosE = std::cos(eps);

    raRad  = std::atan2(sinL * cosE - (sinB / cosB) * sinE, cosL);
    if (raRad < 0) raRad += 2.0 * M_PI;
    decRad = std::asin(sinB * cosE + cosB * sinE * sinL);
}

// Approximate Sun ecliptic longitude (radians).
double approximateSunLongitude(double jd) {
    const double T = (jd - 2451545.0) / 36525.0;
    const double L = std::fmod(280.460 + 36000.770 * T, 360.0);
    const double g = std::fmod(357.528 + 35999.050 * T, 360.0) * M_PI / 180.0;
    return (L + 1.915 * std::sin(g) + 0.020 * std::sin(2.0 * g)) * M_PI / 180.0;
}

// Great-circle angular separation (radians in, degrees out).
double angularSeparationDeg(double ra1, double dec1, double ra2, double dec2) {
    double c = std::sin(dec1) * std::sin(dec2)
             + std::cos(dec1) * std::cos(dec2) * std::cos(ra1 - ra2);
    if (c >  1.0) c =  1.0;
    if (c < -1.0) c = -1.0;
    return std::acos(c) * 180.0 / M_PI;
}

// Local Sidereal Time (radians) at given JD and observer longitude (radians).
double computeLSTRad(double jd, double longitudeRad) {
    const double T = (jd - 2451545.0) / 36525.0;
    double gmst = 280.46061837 + 360.98564736629 * (jd - 2451545.0)
                + 0.000387933 * T * T - T * T * T / 38710000.0;
    gmst = std::fmod(gmst, 360.0);
    if (gmst < 0) gmst += 360.0;
    double lst = gmst + longitudeRad * 180.0 / M_PI;
    lst = std::fmod(lst, 360.0);
    if (lst < 0) lst += 360.0;
    return lst * M_PI / 180.0;
}

// Altitude (radians) of an RA/Dec source at given LST and observer latitude.
double altitudeRad(double raRad, double decRad, double lstRad, double latRad) {
    const double ha = lstRad - raRad;
    double s = std::sin(latRad) * std::sin(decRad)
             + std::cos(latRad) * std::cos(decRad) * std::cos(ha);
    if (s >  1.0) s =  1.0;
    if (s < -1.0) s = -1.0;
    return std::asin(s);
}

// Parallactic angle (radians) for a target at given hour-angle and Dec, for
// an observer at the given latitude. This is the angle between the local
// vertical and the celestial north pole as seen at the target. An alt-az
// camera frame stays fixed in the mount; the sky rotates by Δq over time.
double parallacticAngleRad(double haRad, double decRad, double latRad) {
    const double sinHA = std::sin(haRad);
    const double cosHA = std::cos(haRad);
    const double tanLat = std::tan(latRad);
    const double sinDec = std::sin(decRad);
    const double cosDec = std::cos(decRad);
    return std::atan2(sinHA, cosDec * tanLat - sinDec * cosHA);
}
} // namespace

CelestronOriginSimulator::CelestronOriginSimulator(const QString& dsoFilter, bool rayleighEnabled, double dsoAttenuation, int bortleClass, QObject *parent) : QObject(parent), m_rayleighEnabled(rayleighEnabled), m_bortleClass(bortleClass) {
    // Initialize core components
    m_telescopeState = new TelescopeState();
    m_commandHandler = new CommandHandler(m_telescopeState, this);
    m_statusSender = new StatusSender(m_telescopeState, this);

    // Initialize the dual protocol server
    m_tcpServer = new QTcpServer(this);
    m_udpSocket = new QUdpSocket(this);

    // Initialize Gaia star field renderer (parks at the pole).
    setupGaiaRenderer();

    // --dso=<name> workaround: enable the Stellarium overlay restricted to one
    // matching DSO and re-park the mount at its center, so the App's first
    // live preview already shows the target. Sidesteps both the search-bug
    // crash and the "Can't see stars" caused by a many-DSO overlay.
    if (!dsoFilter.isEmpty()) {
        const QString nebDir =
            "/Applications/Stellarium.app/Contents/Resources/nebulae/default";
        auto* overlay = new StellariumDSOOverlay(nebDir, dsoFilter, dsoAttenuation);
        if (overlay->isAvailable() && overlay->catalogueSize() == 1) {
            double raDeg = 0, decDeg = 0;
            overlay->singleCenterCoords(raDeg, decDeg);

            const double raRad  = raDeg  * M_PI / 180.0;
            const double decRad = decDeg * M_PI / 180.0;
            const double lat    = m_telescopeState->latitude;
            const double jd     = m_telescopeState->computeJD();
            const double lstRad = computeLSTRad(jd, m_telescopeState->longitude);
            const double haRad  = lstRad - raRad;
            double sinAlt = std::sin(lat) * std::sin(decRad)
                          + std::cos(lat) * std::cos(decRad) * std::cos(haRad);
            if (sinAlt >  1.0) sinAlt =  1.0;
            if (sinAlt < -1.0) sinAlt = -1.0;
            const double altRad = std::asin(sinAlt);
            double cosAz = (std::sin(decRad) - std::sin(lat) * sinAlt)
                         / (std::cos(lat) * std::cos(altRad));
            if (cosAz >  1.0) cosAz =  1.0;
            if (cosAz < -1.0) cosAz = -1.0;
            double azRad = std::acos(cosAz);
            if (std::sin(haRad) > 0) azRad = 2.0 * M_PI - azRad;

            m_telescopeState->baseRA  = raRad;
            m_telescopeState->baseDec = decRad;
            m_telescopeState->ra      = raRad;
            m_telescopeState->dec     = decRad;
            m_telescopeState->altitude = altRad;
            m_telescopeState->azimuth  = azRad;
            m_hasParAngleRef = false;   // recapture field-rotation reference
            m_walkRA = 0.0; m_walkDec = 0.0; // discard pole-render drift

            m_dsoOverlay = overlay;
            qDebug() << "DSO startup: parked at" << dsoFilter
                     << QString("RA=%1° Dec=%2° Alt=%3° Az=%4°")
                          .arg(raDeg, 0, 'f', 4).arg(decDeg, 0, 'f', 4)
                          .arg(altRad * 180.0 / M_PI, 0, 'f', 2)
                          .arg(azRad  * 180.0 / M_PI, 0, 'f', 2);
            qDebug() << "DSO will appear only in STACKED_MASTER and SNAPSHOT frames"
                     << "(start imaging in the App to see it).";
            if (altRad < 0)
                qWarning() << "DSO startup: target is below horizon at current time/location";
            renderGaiaImageForPosition(raDeg, decDeg);
        } else {
            qWarning() << "DSO filter" << dsoFilter << "matched"
                       << overlay->catalogueSize()
                       << "tiles — pass a more specific name (e.g. --dso=m51-vasey)";
            delete overlay;
        }
    }
    
    if (m_tcpServer->listen(QHostAddress::Any, SERVER_PORT)) {
        setupConnections();
        setupTimers();
        
        QTimer::singleShot(100, this, &CelestronOriginSimulator::sendBroadcast);
    } else {
        qDebug() << "Failed to start Origin simulator:" << m_tcpServer->errorString();
    }
}

CelestronOriginSimulator::~CelestronOriginSimulator() {
    if (m_tcpServer) {
        m_tcpServer->close();
    }
    qDeleteAll(m_webSocketClients);
    delete m_dsoOverlay;
}

void CelestronOriginSimulator::setupGaiaRenderer() {
    m_gaiaRenderer = new GaiaStarFieldRenderer(this);

    qDebug() << "========================================";
    qDebug() << "Gaia Star Field Renderer initialized";
    qDebug() << "Available:" << m_gaiaRenderer->isAvailable();
    qDebug() << "========================================";

    // Repeatable startup: park at the celestial pole nearest the observer.
    // Northern hemisphere → Polaris (Dec=+90°, Alt=lat, Az=0);
    // Southern hemisphere → Sigma Octantis (Dec=−90°, Alt=−lat, Az=π).
    // RA is degenerate at the pole; set to 0 for determinism.
    {
        const double lat = m_telescopeState->latitude;
        const bool   north = lat >= 0;
        m_telescopeState->baseRA  = 0.0;
        m_telescopeState->baseDec = north ?  M_PI/2 : -M_PI/2;
        m_telescopeState->ra      = m_telescopeState->baseRA;
        m_telescopeState->dec     = m_telescopeState->baseDec;
        m_telescopeState->altitude = north ?  lat   : -lat;
        m_telescopeState->azimuth  = north ?  0.0   :  M_PI;
    }

    double ra_deg = m_telescopeState->ra * 180.0 / M_PI;
    double dec_deg = m_telescopeState->dec * 180.0 / M_PI;
    qDebug() << QString("Starting position (celestial pole): RA=%1 Dec=%2 Alt=%3 Az=%4")
                .arg(ra_deg, 0, 'f', 4).arg(dec_deg, 0, 'f', 4)
                .arg(m_telescopeState->altitude * 180.0 / M_PI, 0, 'f', 2)
                .arg(m_telescopeState->azimuth * 180.0 / M_PI, 0, 'f', 2);

    renderGaiaImageForPosition(ra_deg, dec_deg);
}

void CelestronOriginSimulator::renderGaiaImageForPosition(double ra_deg, double dec_deg) {
    // Mount-error model — applied before the render so what the App receives
    // matches what a real (imperfect) mount would have actually imaged.
    //   Periodic error: sinusoidal worm-gear drift, ±5″ amplitude, 8-min period.
    //   Walking noise:  σ=0.5″ random step per frame, accumulating (so the
    //                   stacked master would slowly drift through the session).
    constexpr double pe_amp_arcsec    = 0.5;   // measured: ~0.5″ at 262s period
    constexpr double pe_period_sec    = 260.0; // measured worm-cycle in real Origin
    constexpr double walk_sigma_arcsec = 0.1;  // derived from 1.3″ RMS over 393 frames

    // Clamp |cosDec| to a meaningful minimum (≈85° from equator). Real mounts
    // don't accumulate unbounded RA drift at the pole — the gimbal can't
    // express it. The earlier 1e-6 clamp produced ~10^6× amplified walking
    // noise at dec=90° and wrecked the pointing within a single frame.
    double cosDec = std::cos(dec_deg * M_PI / 180.0);
    constexpr double cosDecFloor = 0.1;
    if (std::fabs(cosDec) < cosDecFloor)
        cosDec = (cosDec < 0 ? -cosDecFloor : cosDecFloor);

    const double elapsed = m_simStart.msecsTo(QDateTime::currentDateTime()) * 1e-3;
    const double pe_arcsec = pe_amp_arcsec * std::sin(2.0 * M_PI * elapsed / pe_period_sec);

    m_walkRA  += gaussian() * walk_sigma_arcsec / 3600.0 / cosDec;
    m_walkDec += gaussian() * walk_sigma_arcsec / 3600.0;

    const double ra_actual  = ra_deg  + pe_arcsec / 3600.0 / cosDec + m_walkRA;
    const double dec_actual = dec_deg + m_walkDec;

    // Sky conditions — Rayleigh sky brightening (from pointing altitude) and
    // moonglow (from Moon position relative to pointing). Approximate Moon
    // ephemeris is good to ~0.1°, far better than the multi-degree halo width.
    SkyConditions sky;
    sky.altitudeDeg     = m_telescopeState->altitude * 180.0 / M_PI;
    sky.rayleighEnabled = m_rayleighEnabled;
    sky.bortleClass     = m_bortleClass;
    sky.exposureSec     = std::max(0.0, m_telescopeState->exposure);

    const double jd = m_telescopeState->computeJD();
    double moonRa, moonDec;
    approximateMoonRaDec(jd, moonRa, moonDec);

    const double lstRad = computeLSTRad(jd, m_telescopeState->longitude);
    const double moonAlt = altitudeRad(moonRa, moonDec, lstRad, m_telescopeState->latitude);
    sky.moonUp = (moonAlt > 0.0);

    // Alt-az field rotation: capture the parallactic angle at the first render
    // of each session (or after a slew) as the reference, then rotate the
    // projected star field by the change since that reference on every frame.
    const double haRad = lstRad - ra_actual * M_PI / 180.0;
    const double decRadActual = dec_actual * M_PI / 180.0;
    const double parAngleRad = parallacticAngleRad(haRad, decRadActual, m_telescopeState->latitude);
    if (!m_hasParAngleRef) {
        m_referenceParAngleRad = parAngleRad;
        m_hasParAngleRef = true;
    }
    const double fieldRotDeg = (parAngleRad - m_referenceParAngleRad) * 180.0 / M_PI;

    // Trailing: the field rotates *during* the exposure too. Compute par-angle
    // at the start and end of the shutter-open period and use the difference
    // as the per-exposure trail rotation. Sidereal angular rate is ~7.29e-5
    // rad/s — HA advances by that times exposure duration. Using the same
    // parallactic-angle formula keeps the sign convention consistent with the
    // session-rotation reference.
    constexpr double siderealRate = 2.0 * M_PI / 86164.0905;     // rad/sidereal-sec
    const double exposureSec = std::max(0.0, m_telescopeState->exposure);
    const double dtHA = siderealRate * exposureSec / 2.0;
    const double parStart = parallacticAngleRad(haRad - dtHA, decRadActual, m_telescopeState->latitude);
    const double parEnd   = parallacticAngleRad(haRad + dtHA, decRadActual, m_telescopeState->latitude);
    double exposureRotDeg = (parEnd - parStart) * 180.0 / M_PI;
    while (exposureRotDeg >  180.0) exposureRotDeg -= 360.0;
    while (exposureRotDeg < -180.0) exposureRotDeg += 360.0;

    if (sky.moonUp) {
        // Phase: illuminated fraction = (1 - cos(elongation)) / 2, where
        // elongation is the Moon's angular distance from the Sun.
        const double sunLon = approximateSunLongitude(jd);
        const double elongation = std::acos(std::cos(moonRa - sunLon)); // approximation
        sky.moonPhase = 0.5 * (1.0 - std::cos(elongation));

        sky.moonSepDeg = angularSeparationDeg(
            ra_actual * M_PI / 180.0, dec_actual * M_PI / 180.0,
            moonRa, moonDec);
    }

    qDebug() << QString("Rendering Gaia field: RA=%1 Dec=%2  (PE %3″, walk %4″/%5″)  "
                        "alt=%6°  moonSep=%7°  phase=%8  moonUp=%9  fieldRot=%10°  expRot=%11°")
                .arg(ra_actual, 0, 'f', 6).arg(dec_actual, 0, 'f', 6)
                .arg(pe_arcsec, 0, 'f', 2)
                .arg(m_walkRA * 3600.0, 0, 'f', 2)
                .arg(m_walkDec * 3600.0, 0, 'f', 2)
                .arg(sky.altitudeDeg, 0, 'f', 1)
                .arg(sky.moonSepDeg, 0, 'f', 1)
                .arg(sky.moonPhase, 0, 'f', 2)
                .arg(sky.moonUp ? "yes" : "no")
                .arg(fieldRotDeg, 0, 'f', 2)
                .arg(exposureRotDeg, 0, 'f', 3);

    // Only paint DSOs into accumulating-exposure frames (STACKED_MASTER) and
    // one-shot snapshots — not LIVE previews. Two reasons: (1) the App runs
    // its star detector on LIVE previews during alignment and nebulosity
    // there triggers "Can't see stars" / crashes; (2) a real telescope's
    // short-exposure LIVE preview shows only stars too — the nebula only
    // emerges after many stacked exposures. m_pendingSnapshotNotify is the
    // existing flag that distinguishes the two.
    const StellariumDSOOverlay* overlayForThisFrame =
        m_pendingSnapshotNotify ? m_dsoOverlay : nullptr;

    QByteArray tiff = m_gaiaRenderer->renderField(
        ra_actual, dec_actual, OUTPUT_WIDTH, OUTPUT_HEIGHT, PIXSCALE_ARCSEC,
        sky, fieldRotDeg, exposureRotDeg, overlayForThisFrame,
        std::max(1, m_telescopeState->stackDepth));

    if (!tiff.isEmpty()) {
        onImageReady(tiff);
    } else {
        qWarning() << "Gaia render returned empty — no stars in region";
    }
}

void CelestronOriginSimulator::onImageReady(const QByteArray& tiffData) {
    qDebug() << "Gaia TIFF ready:" << tiffData.size() << "bytes";

    // Route the render into the right cache so DSO-bearing stacked masters
    // don't leak into what the App / PixInsight see as the LIVE preview. The
    // m_pendingSnapshotNotify flag is true when updateImaging or the
    // snapshot handler set up a STACKED_MASTER / SNAPSHOT path; false for
    // initial / slew-complete LIVE renders.
    if (m_pendingSnapshotNotify) {
        m_imageDataStack = tiffData;
        rebuildPreviewJpeg(m_imageDataStack, m_previewJpegStack, "stack");
    } else {
        m_imageDataLive = tiffData;
        rebuildPreviewJpeg(m_imageDataLive, m_previewJpegLive, "live");
    }

    if (m_pendingSnapshotNotify) {
        // Distinguish continuous imaging (STACKED_MASTER, .jpg) from one-shot
        // RunSampleCapture (SNAPSHOT, .tiff). The path tells us which.
        if (m_pendingSnapshotPath.contains("StackedMaster"))
            m_telescopeState->imageType = "STACKED_MASTER";
        else
            m_telescopeState->imageType = "SNAPSHOT";
        m_telescopeState->fileLocation = m_pendingSnapshotPath;
        m_pendingSnapshotNotify = false;
        qDebug() << m_telescopeState->imageType << "ready:"
                 << m_telescopeState->fileLocation;
    } else {
        m_telescopeState->fileLocation = m_telescopeState->getNextImageFile();
        m_telescopeState->imageType = "LIVE";
        m_telescopeState->sequenceNumber++;
    }

    // Notify clients
    m_statusSender->sendNewImageReadyToAll();

    // After sending the one-off notification, reset to normal live mode so
    // the next LIVE preview broadcast uses Temp/N.jpg semantics again.
    if (m_telescopeState->imageType == "SNAPSHOT"
     || m_telescopeState->imageType == "STACKED_MASTER") {
        m_telescopeState->imageType = "LIVE";
        m_telescopeState->fileLocation.clear();
    }
}

void CelestronOriginSimulator::rebuildPreviewJpeg(const QByteArray& tiffSrc, QByteArray& jpegDst, const char* label) {
    // The renderer's TIFF layout (GaiaStarFieldRenderer::write16BitRGBTiff) is
    // header(8) + interleaved RGB uint16 little-endian pixels + IFD. We pull
    // the pixel data straight from the buffer rather than re-decoding via
    // QImage::loadFromData("TIFF"), because the renderer's hand-written 16-bit
    // uncompressed TIFF isn't reliably handled by Qt's TIFF plugin.
    constexpr int pixelOffset = 8;
    constexpr int w    = OUTPUT_WIDTH;
    constexpr int h    = OUTPUT_HEIGHT;
    constexpr int npix = w * h;
    constexpr qsizetype needed = pixelOffset + qsizetype(npix) * 3 * 2;
    if (tiffSrc.size() < needed) {
        jpegDst.clear();
        return;
    }

    const quint16* src = reinterpret_cast<const quint16*>(
        tiffSrc.constData() + pixelOffset);

    // Linear ADU → stretched 8-bit. Calibrated against the real Origin's
    // sensor baseline (bias+amp+dark ~3400 ADU, sky-glow another ~1500-2700
    // depending on Bayer channel). bg clips at the bias floor so it doesn't
    // show in the output; midtone is set so the typical 5000-ADU sky pedestal
    // lands at ~24/255 in the JPEG (matching real-telescope preview JPEGs).
    constexpr float bg       = 0.052f;   // clip at ~3400 ADU bias floor
    constexpr float midtone  = 0.20f;    // maps sky 5000 ADU → ~24/255 output
    constexpr float invSpan  = 1.0f / (1.0f - bg);
    constexpr float inv65535 = 1.0f / 65535.0f;

    QImage img(w, h, QImage::Format_RGB888);
    for (int y = 0; y < h; ++y) {
        uchar* dst = img.scanLine(y);
        for (int x = 0; x < w; ++x) {
            const float r = (src[0] * inv65535 - bg) * invSpan;
            const float g = (src[1] * inv65535 - bg) * invSpan;
            const float b = (src[2] * inv65535 - bg) * invSpan;
            dst[0] = uchar(mtf(r, midtone) * 255.0f + 0.5f);
            dst[1] = uchar(mtf(g, midtone) * 255.0f + 0.5f);
            dst[2] = uchar(mtf(b, midtone) * 255.0f + 0.5f);
            dst += 3;
            src += 3;
        }
    }

    QByteArray jpeg;
    QBuffer buf(&jpeg);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "JPEG", 90);
    jpegDst = jpeg;
    qDebug() << "Preview JPEG built [" << label << "]:" << jpegDst.size() << "bytes";
}

void CelestronOriginSimulator::updateImaging() {
    // Called every 1s while RunImaging is active. We count down imagingTimeLeft
    // (seconds remaining in the current exposure) and on completion render a
    // snapshot TIFF, broadcast NewImageReady, then start the next exposure.
    if (!m_telescopeState->isImaging) {
        m_imagingTimer->stop();
        return;
    }

    // Advance the App's progress bar.
    m_telescopeState->exposedTime += 1.0;

    if (--m_telescopeState->imagingTimeLeft > 0)
        return;

    // Exposure complete — emit a STACKED_MASTER frame following the real
    // Origin convention: rolling-buffer filename inside a per-session
    // directory, incrementing StackDepth. The App distinguishes these from
    // SNAPSHOT (which the snapshotRequested path still produces).
    m_telescopeState->stackDepth++;
    const int slot = (m_telescopeState->stackDepth - 1) % 5;
    m_pendingSnapshotPath = QString("Images/Astrophotography/%1/StackedMaster%2.jpg")
        .arg(m_telescopeState->imagingSessionDir)
        .arg(slot);
    m_pendingSnapshotNotify = true;

    renderGaiaImageForPosition(
        m_telescopeState->ra * 180.0 / M_PI,
        m_telescopeState->dec * 180.0 / M_PI);

    // Restart timer for the next exposure (continuous imaging until
    // CancelImaging / HaltTasks).
    int next = static_cast<int>(m_telescopeState->exposure);
    if (next < 1) next = 1;
    m_telescopeState->imagingTimeLeft = next;
    m_telescopeState->exposedTime = 0.0;
}

void CelestronOriginSimulator::updateSlew() {
    static int slewProgress = 0;
    static int callCount = 0;
    callCount++;
    
    // Debug: Print every 50 calls (every 5 seconds)
    if (callCount % 50 == 0) {
        qDebug() << "updateSlew() called" << callCount << "times";
    }
    
    // Update slew motion if active

    if (m_telescopeState->isSlewing) {
      m_telescopeState->updateSlewMotion();
    }

    slewProgress += 20;
    
    if (slewProgress >= 100) {
        qDebug() << "Before update - RA:" << m_telescopeState->ra << "Dec:" << m_telescopeState->dec;
        qDebug() << "Target RA:" << m_telescopeState->targetRa << "Target Dec:" << m_telescopeState->targetDec;
        
        // Slew complete
        m_telescopeState->isGotoOver = true;
        m_telescopeState->isSlewing = false;
        m_telescopeState->baseRA = m_telescopeState->targetRa;
        m_telescopeState->baseDec = m_telescopeState->targetDec;
        m_telescopeState->ra = m_telescopeState->baseRA;
        m_telescopeState->dec = m_telescopeState->baseDec;
        m_hasParAngleRef = false; // capture a fresh field-rotation reference at the new target
        m_walkRA = 0.0; m_walkDec = 0.0; // fresh target → reset accumulated mount drift
        
        // Stop the timer
        m_slewTimer->stop();
        slewProgress = 0;

        qDebug() << "After update - RA:" << m_telescopeState->ra << "Dec:" << m_telescopeState->dec;
        
        // Update mount status
        m_statusSender->sendMountStatusToAll();

        // Render Gaia star field for new position
        QTimer::singleShot(100, this, [this]() {
            renderGaiaImageForPosition(
                m_telescopeState->targetRa * 180.0 / M_PI,
                m_telescopeState->targetDec * 180.0 / M_PI);
        });
        
        qDebug() << "🎯 Slew complete";
    }
}

void CelestronOriginSimulator::updateAltAzSlew() {
    if (!m_telescopeState->isAltAzSlewing) {
        m_altAzSlewTimer->stop();
        return;
    }

    // Move towards target alt/az at ~10 deg/s (in 100ms steps = 1 deg per tick)
    const double SLEW_RATE = 1.0 * M_PI / 180.0;  // 1 degree per 100ms tick = 10 deg/s

    double dAlt = m_telescopeState->targetAltitude - m_telescopeState->altitude;
    double dAzm = m_telescopeState->targetAzimuth - m_telescopeState->azimuth;

    // Wrap azimuth difference to [-pi, pi]
    while (dAzm > M_PI) dAzm -= 2.0 * M_PI;
    while (dAzm < -M_PI) dAzm += 2.0 * M_PI;

    double dist = sqrt(dAlt * dAlt + dAzm * dAzm);

    if (dist < 0.002) {  // ~0.1 degree — close enough
        // Slew complete
        m_telescopeState->altitude = m_telescopeState->targetAltitude;
        m_telescopeState->azimuth = m_telescopeState->targetAzimuth;

        // Wrap azimuth to [0, 2pi]
        while (m_telescopeState->azimuth < 0) m_telescopeState->azimuth += 2.0 * M_PI;
        while (m_telescopeState->azimuth >= 2.0 * M_PI) m_telescopeState->azimuth -= 2.0 * M_PI;

        // Update RA/Dec from new Alt/Az
        m_telescopeState->altAzToRaDec();

        m_telescopeState->isAltAzSlewing = false;
        m_telescopeState->isGotoOver = true;
        m_hasParAngleRef = false; // capture a fresh field-rotation reference at the new target
        m_walkRA = 0.0; m_walkDec = 0.0; // fresh target → reset accumulated mount drift

        m_altAzSlewTimer->stop();

        qDebug() << QString("GotoAltAzm complete: Alt=%1 deg  Azm=%2 deg  RA=%3 rad  Dec=%4 rad")
                    .arg(m_telescopeState->altitude * 180.0 / M_PI, 0, 'f', 2)
                    .arg(m_telescopeState->azimuth * 180.0 / M_PI, 0, 'f', 2)
                    .arg(m_telescopeState->ra, 0, 'f', 6)
                    .arg(m_telescopeState->dec, 0, 'f', 6);

        m_statusSender->sendMountStatusToAll();

        // Render star field for new position
        QTimer::singleShot(100, this, [this]() {
            renderGaiaImageForPosition(
                m_telescopeState->ra * 180.0 / M_PI,
                m_telescopeState->dec * 180.0 / M_PI);
        });
    } else {
        // Move proportionally toward target, capped at SLEW_RATE
        double scale = std::min(1.0, SLEW_RATE / dist);
        m_telescopeState->altitude += dAlt * scale;
        m_telescopeState->azimuth += dAzm * scale;

        // Clamp altitude
        const double MAX_ALT = M_PI / 2.0;
        const double MIN_ALT = -M_PI / 2.0;
        if (m_telescopeState->altitude > MAX_ALT) m_telescopeState->altitude = MAX_ALT;
        if (m_telescopeState->altitude < MIN_ALT) m_telescopeState->altitude = MIN_ALT;

        // Wrap azimuth
        while (m_telescopeState->azimuth < 0) m_telescopeState->azimuth += 2.0 * M_PI;
        while (m_telescopeState->azimuth >= 2.0 * M_PI) m_telescopeState->azimuth -= 2.0 * M_PI;

        // Update RA/Dec
        m_telescopeState->altAzToRaDec();
    }
}

void CelestronOriginSimulator::setupConnections() {
    connect(m_tcpServer, &QTcpServer::newConnection, this, &CelestronOriginSimulator::handleNewConnection);

    // Connect command handler signals
    connect(m_commandHandler, &CommandHandler::slewStarted, this, [this]() {
        m_slewTimer->start(500);
    });

    // GotoAltAzm slew timer: drives smooth alt/az motion at 100ms ticks
    connect(m_commandHandler, &CommandHandler::altAzSlewStarted, this, [this]() {
        m_altAzSlewTimer->start(100);
    });

    // StopAll: halt every timer and motion state
    connect(m_commandHandler, &CommandHandler::allStopped, this, [this]() {
        m_slewTimer->stop();
        m_altAzSlewTimer->stop();
        m_manualSlewTimer->stop();
        m_statusSender->sendMountStatusToAll();
    });

    // Manual slew timer: drives updateSlewMotion at 100ms ticks
    connect(m_commandHandler, &CommandHandler::manualSlewStarted, this, [this]() {
        if (!m_manualSlewTimer->isActive())
            m_manualSlewTimer->start(100);
    });
    connect(m_commandHandler, &CommandHandler::manualSlewStopped, this, [this]() {
        m_manualSlewTimer->stop();
    });

    // Snapshot: render Gaia field for current position after simulated exposure
    connect(m_commandHandler, &CommandHandler::snapshotRequested, this,
            [this](double exposure, int /*iso*/, int /*binning*/) {
        int delayMs = qBound(500, static_cast<int>(exposure * 1000), 10000);
        QTimer::singleShot(delayMs, this, [this]() {
            m_pendingSnapshotPath = QString("Images/Temp/snapshot_%1.tiff")
                .arg(m_telescopeState->sequenceNumber);
            m_telescopeState->sequenceNumber++;
            m_pendingSnapshotNotify = true;

            // Gaia rendering is synchronous — no async wait
            renderGaiaImageForPosition(
                m_telescopeState->ra * 180.0 / M_PI,
                m_telescopeState->dec * 180.0 / M_PI);
        });
    });

    connect(m_commandHandler, &CommandHandler::imagingStarted, this, [this]() {
        m_imagingTimer->start(1000);
    });

    // Connect initialization signal
    connect(m_commandHandler, &CommandHandler::initializationStarted, this, [this](bool fakeInit) {
        if (fakeInit) {
            QTimer::singleShot(1000, this, &CelestronOriginSimulator::completeInitialization);
        } else {
            m_initTimer->start(3000);
        }
    });
}

void CelestronOriginSimulator::setupTimers() {
    // Create broadcast timer
    m_broadcastTimer = new QTimer(this);
    connect(m_broadcastTimer, &QTimer::timeout, this, &CelestronOriginSimulator::sendBroadcast);
    m_broadcastTimer->start(BROADCAST_INTERVAL);
    
    // Create update timer for regular status updates
    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, &QTimer::timeout, this, &CelestronOriginSimulator::sendStatusUpdates);
    m_updateTimer->start(1000);
    
    // Create slew timer (GotoRaDec)
    m_slewTimer = new QTimer(this);
    connect(m_slewTimer, &QTimer::timeout, this, &CelestronOriginSimulator::updateSlew);

    // Create alt/az slew timer (GotoAltAzm, 100ms ticks)
    m_altAzSlewTimer = new QTimer(this);
    connect(m_altAzSlewTimer, &QTimer::timeout, this, &CelestronOriginSimulator::updateAltAzSlew);

    // Create manual slew timer (Slew with rates, 100ms ticks)
    m_manualSlewTimer = new QTimer(this);
    connect(m_manualSlewTimer, &QTimer::timeout, this, [this]() {
        m_telescopeState->updateSlewMotion();
    });

    // Create imaging timer
    m_imagingTimer = new QTimer(this);
    connect(m_imagingTimer, &QTimer::timeout, this, &CelestronOriginSimulator::updateImaging);

    // Create initialization timer
    m_initTimer = new QTimer(this);
    m_initTimer->setSingleShot(false);
    connect(m_initTimer, &QTimer::timeout, this, &CelestronOriginSimulator::updateInitialization);
}


void CelestronOriginSimulator::updateInitialization() {
    // Simulate real telescope initialization progression through stages:
    // FOCUSING (0-35%) -> MOVING_MOUNT (35-50%) -> ALIGNING point 1 (50-75%) -> ALIGNING point 2 (75-100%)
    m_initUpdateCount++;

    // Stage 1: FOCUSING (counts 1-5, ~0-35%)
    if (m_initUpdateCount <= 5) {
        m_telescopeState->initInfo.currentStep = "FOCUSING";
        m_telescopeState->initInfo.percentageComplete = m_initUpdateCount * 7; // 7, 14, 21, 28, 35
        m_telescopeState->focusInfo.percentageComplete = m_initUpdateCount * 20; // 20, 40, 60, 80, 100

        // Focus position found at count 5
        if (m_initUpdateCount == 5) {
            m_telescopeState->focusInfo.position = 18617;
            m_telescopeState->initInfo.positionOfFocus = 18617;
        }
    }
    // Stage 2: MOVING_MOUNT (counts 6-8, ~36-50%)
    else if (m_initUpdateCount <= 8) {
        m_telescopeState->initInfo.currentStep = "MOVING_MOUNT";
        m_telescopeState->initInfo.percentageComplete = 35 + (m_initUpdateCount - 5) * 5; // 40, 45, 50
    }
    // Stage 3: ALIGNING first point (counts 9-11, ~50-75%)
    else if (m_initUpdateCount <= 11) {
        m_telescopeState->initInfo.currentStep = "ALIGNING";
        m_telescopeState->initInfo.numPoints = 1;
        m_telescopeState->initInfo.numPointsRemaining = 1;
        m_telescopeState->initInfo.percentageComplete = 50 + (m_initUpdateCount - 8) * 8; // 58, 66, 74
    }
    // Stage 4: ALIGNING second point (counts 12-15, ~75-100%)
    else if (m_initUpdateCount <= 15) {
        m_telescopeState->initInfo.currentStep = "ALIGNING";
        m_telescopeState->initInfo.numPoints = 2;
        m_telescopeState->initInfo.numPointsRemaining = 0;
        m_telescopeState->initInfo.percentageComplete = 74 + (m_initUpdateCount - 11) * 7; // 81, 88, 95, 100
        if (m_initUpdateCount == 15) {
            m_telescopeState->initInfo.percentageComplete = 100;
        }
    }

    // Send status update with progress
    m_statusSender->sendTaskControllerStatusToAll();

    // Randomly decide if initialization fails (~10% chance per tick before alignment)
    if (m_initUpdateCount < 9 && QRandomGenerator::global()->bounded(100) < 10) {
        failInitialization();
        return;
    }

    // Complete the initialization
    if (m_initUpdateCount >= 15) {
        completeInitialization();
    }
}

void CelestronOriginSimulator::completeInitialization() {
    // Stop the timer
    m_initTimer->stop();
    m_initUpdateCount = 0;
    
    // Set final status
    m_telescopeState->isInitializing = false;
    m_telescopeState->stage = "COMPLETE";
    m_telescopeState->isReady = true;
    
    // Send completion status
    m_statusSender->sendTaskControllerStatusToAll();
    
    // Wait a moment and transition to IDLE state
    QTimer::singleShot(1000, this, [this]() {
        m_telescopeState->state = "IDLE";
        m_statusSender->sendTaskControllerStatusToAll();
    });
}

void CelestronOriginSimulator::failInitialization() {
    // Stop the timer
    m_initTimer->stop();
    m_initUpdateCount = 0;
    
    // Set failure status
    m_telescopeState->isInitializing = false;
    m_telescopeState->stage = "STOPPED";
    m_telescopeState->isReady = false;
    
    // Send error notification
    QJsonObject errorNotification;
    errorNotification["Command"] = "Error";
    errorNotification["Destination"] = "All";
    errorNotification["ErrorCode"] = -78;
    errorNotification["ErrorMessage"] = "Initialization failed. Please point the scope away from any bright lights; buildings; trees and try again.";
    errorNotification["ExpiredAt"] = m_telescopeState->getExpiredAt();
    errorNotification["Type"] = "Notification";
    m_statusSender->sendJsonMessageToAll(errorNotification);
    
    // Send failure status
    m_statusSender->sendTaskControllerStatusToAll();
}

// Rest of the existing methods remain the same...
void CelestronOriginSimulator::handleNewConnection() {
    QTcpSocket *socket = m_tcpServer->nextPendingConnection();
    
    // Store pending request data
    m_pendingRequests[socket] = QByteArray();
    
    // CRITICAL: Use QueuedConnection to prevent immediate processing conflicts
    connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
        handleIncomingData(socket);
    }, Qt::QueuedConnection);
    
    connect(socket, &QTcpSocket::disconnected, this, [this, socket]() {
        m_pendingRequests.remove(socket);
        socket->deleteLater();
    });
}

// Include the rest of your existing methods here...
// (handleIncomingData, handleWebSocketUpgrade, etc.)
// They remain unchanged from your original implementation

void CelestronOriginSimulator::sendBroadcast() {
    // Prepare the broadcast message
    QString message = QString("Identity:Origin-%1Z Origin IP Address = %2").arg(broadcast_id);
    
    // Get our IP addresses
    QList<QHostAddress> ipAddresses = QNetworkInterface::allAddresses();
    
    for (const QHostAddress &address : ipAddresses) {
        if (address.protocol() == QAbstractSocket::IPv4Protocol && address != QHostAddress::LocalHost) {
            QString broadcastMessage = message.arg(address.toString());
            
            // Send broadcast on all network interfaces
            m_udpSocket->writeDatagram(
                broadcastMessage.toUtf8(),
                QHostAddress::Broadcast,
                BROADCAST_PORT
            );
        }
    }
}


void CelestronOriginSimulator::handleIncomingData(QTcpSocket *socket) {
    QByteArray newData = socket->readAll();
    m_pendingRequests[socket].append(newData);
    
    QByteArray &requestData = m_pendingRequests[socket];
    
    // Look for complete HTTP headers
    int headerEndPos = requestData.indexOf("\r\n\r\n");
    if (headerEndPos == -1) {
        // Headers not complete yet, wait for more data
        if (requestData.size() > 8192) {
            // Too much data without finding headers, disconnect
            socket->disconnectFromHost();
            return;
        }
        return;
    }
    
    // We have complete headers, determine the protocol
    QString request = QString::fromUtf8(requestData.left(headerEndPos));
    QStringList lines = request.split("\r\n");
    
    if (lines.isEmpty()) {
        socket->disconnectFromHost();
        return;
    }
    
    QString requestLine = lines[0];
    QStringList requestParts = requestLine.split(" ");
    
    if (requestParts.size() < 3) {
        socket->disconnectFromHost();
        return;
    }
    
    QString method = requestParts[0];
    QString path = requestParts[1];
    
//     qDebug() << "Origin Protocol Request:" << method << path;
    
    // Check if this is a WebSocket upgrade request
    bool isWebSocketUpgrade = false;
    for (const QString &line : lines) {
        if (line.toLower().contains("upgrade: websocket")) {
            isWebSocketUpgrade = true;
            break;
        }
    }
    
    if (isWebSocketUpgrade && path == "/SmartScope-1.0/mountControlEndpoint") {
        // Handle WebSocket upgrade for telescope control
        handleWebSocketUpgrade(socket, requestData);
    } else if (method == "GET" && path.startsWith("/SmartScope-1.0/dev2/Images/Temp/")) {
        // Handle HTTP image request
        handleHttpImageRequest(socket, path);
    } else if (method == "GET" && path.contains("/SmartScope-1.0/dev2/Images/Astrophotography/")) {
        // Handle HTTP astrophotography image request
        handleHttpAstroImageRequest(socket, path);
    } else {
        // Unknown request
        sendHttpResponse(socket, 404, "text/plain", "Not Found");
    }
    
    // Clear the pending request data
    m_pendingRequests.remove(socket);
}
// CORRECTED FIX: CelestronOriginSimulator.cpp - Proper handshake sequence
// Perform handshake FIRST, then transfer socket ownership

void CelestronOriginSimulator::handleWebSocketUpgrade(QTcpSocket *socket, const QByteArray &requestData) {
//     // qDebug() << "*** STARTING WEBSOCKET UPGRADE PROCESS ***";
//     qDebug() << "Request data size:" << requestData.size();
    
    // Create WebSocketConnection but DON'T disconnect protocol detector yet
    WebSocketConnection *wsConn = new WebSocketConnection(socket, this, false); // false = don't take ownership yet
    
    // FIRST: Perform the handshake using the request data
    if (wsConn->performHandshake(requestData)) {
//         // qDebug() << "*** HANDSHAKE SUCCESSFUL - TRANSFERRING SOCKET OWNERSHIP ***";
        
        // CRITICAL: NOW disconnect the protocol detector since handshake worked
        disconnect(socket, &QTcpSocket::readyRead, this, nullptr);
//         // qDebug() << "*** PROTOCOL DETECTOR DISCONNECTED ***";
        
        // Clear any pending data since we're switching protocols  
        m_pendingRequests.remove(socket);
        
        // NOW let WebSocketConnection take full ownership
        wsConn->takeSocketOwnership();
        
        // Add to our client list
        m_webSocketClients.append(wsConn);
        m_statusSender->addWebSocketClient(wsConn);
        
        // Set up all signal connections for WebSocket handling
        connect(wsConn, &WebSocketConnection::textMessageReceived, 
                this, &CelestronOriginSimulator::processWebSocketCommand);
        
        connect(wsConn, &WebSocketConnection::pingReceived,
                this, &CelestronOriginSimulator::handleWebSocketPing);
        
        connect(wsConn, &WebSocketConnection::pongReceived,
                this, &CelestronOriginSimulator::handleWebSocketPong);
        
        connect(wsConn, &WebSocketConnection::pingTimeout,
                this, &CelestronOriginSimulator::handleWebSocketTimeout);
        
        connect(wsConn, &WebSocketConnection::disconnected, 
                this, &CelestronOriginSimulator::onWebSocketDisconnected);
        
//         qDebug() << "WebSocket connection established for telescope control";
        
        // Send initial status updates after a brief delay
        QTimer::singleShot(1000, this, [this, wsConn]() {
            if (m_webSocketClients.contains(wsConn)) {
                m_statusSender->sendMountStatus(wsConn);
                m_statusSender->sendFocuserStatus(wsConn);
                m_statusSender->sendCameraParams(wsConn);
                m_statusSender->sendDiskStatus(wsConn);
                m_statusSender->sendTaskControllerStatus(wsConn);
                m_statusSender->sendEnvironmentStatus(wsConn);
                m_statusSender->sendDewHeaterStatus(wsConn);
                m_statusSender->sendOrientationStatus(wsConn);
            }
        });
    } else {
//         // qDebug() << "*** HANDSHAKE FAILED - KEEPING PROTOCOL DETECTOR ***";
        
        // Handshake failed, so keep the original protocol detector connected
        // Don't disconnect anything - let it continue as HTTP
        sendHttpResponse(socket, 400, "text/plain", "Bad WebSocket Request");
        
        // Clean up the failed WebSocket connection
        delete wsConn;
    }
}

void CelestronOriginSimulator::handleHttpImageRequest(QTcpSocket *socket, const QString &path) {
    // Images/Temp/* — LIVE-preview path. Serves only the LIVE cache so
    // DSO-bearing stack renders don't leak into what the App treats as the
    // live preview (the App runs its star detector on these).
    const bool wantsTiff = path.endsWith(".tiff", Qt::CaseInsensitive)
                        || path.endsWith(".tif",  Qt::CaseInsensitive);
    if (wantsTiff) {
        if (m_imageDataLive.isEmpty()) {
            sendHttpResponse(socket, 404, "text/plain", "Image not yet rendered");
            return;
        }
        sendHttpResponse(socket, 200, "image/tiff", m_imageDataLive);
        return;
    }
    if (m_previewJpegLive.isEmpty()) {
        sendHttpResponse(socket, 404, "text/plain", "Preview not yet available");
        return;
    }
    sendHttpResponse(socket, 200, "image/jpeg", m_previewJpegLive);
}

void CelestronOriginSimulator::handleHttpAstroImageRequest(QTcpSocket *socket, const QString &path) {
    QStringList pathParts = path.split("/");
    if (pathParts.size() < 6) {
        sendHttpResponse(socket, 404, "text/plain", "Invalid path");
        return;
    }

    const QString directory = pathParts[pathParts.size() - 2];
    const QString fileName  = pathParts.last();
    const QString fullPath  = QString("simulator_data/Images/Astrophotography/%1/%2")
                                .arg(directory, fileName);
    const bool wantsTiff = fileName.endsWith(".tiff", Qt::CaseInsensitive)
                        || fileName.endsWith(".tif",  Qt::CaseInsensitive);

    // Prefer a pre-staged file on disk (fixture scenarios); otherwise fall
    // back to the latest in-memory render (raw TIFF) or the cached stretched
    // preview (JPEG). A disk file is served as-is, matching the requested
    // extension's content type.
    QFile imageFile(fullPath);
    if (imageFile.exists() && imageFile.open(QIODevice::ReadOnly)) {
        QByteArray imageData = imageFile.readAll();
        imageFile.close();
        sendHttpResponse(socket, 200,
                         wantsTiff ? "image/tiff" : "image/jpeg",
                         imageData);
        return;
    }

    // Astrophotography path → STACK cache (DSO-bearing). Fall back to LIVE
    // cache on the rare case nothing has been imaged yet, so the App doesn't
    // see a hard 404 for an Astrophotography URL before imaging starts.
    const QByteArray& tiffCache = !m_imageDataStack.isEmpty()  ? m_imageDataStack  : m_imageDataLive;
    const QByteArray& jpegCache = !m_previewJpegStack.isEmpty() ? m_previewJpegStack : m_previewJpegLive;
    if (wantsTiff) {
        if (tiffCache.isEmpty()) {
            sendHttpResponse(socket, 404, "text/plain", "Image not yet rendered");
            return;
        }
        sendHttpResponse(socket, 200, "image/tiff", tiffCache);
        return;
    }
    if (jpegCache.isEmpty()) {
        sendHttpResponse(socket, 404, "text/plain", "Preview not yet available");
        return;
    }
    sendHttpResponse(socket, 200, "image/jpeg", jpegCache);
}

void CelestronOriginSimulator::sendHttpResponse(QTcpSocket *socket, int statusCode,
                     const QString &contentType, const QByteArray &data) {
    QString statusText;
    switch (statusCode) {
        case 200: statusText = "OK"; break;
        case 404: statusText = "Not Found"; break;
        case 400: statusText = "Bad Request"; break;
        case 500: statusText = "Internal Server Error"; break;
        default: statusText = "Unknown"; break;
    }
    
    QString response = QString("HTTP/1.1 %1 %2\r\n").arg(statusCode).arg(statusText);
    response += QString("Content-Type: %1\r\n").arg(contentType);
    response += QString("Content-Length: %1\r\n").arg(data.size());
    response += "Cache-Control: no-cache\r\n";
    response += "Access-Control-Allow-Origin: *\r\n";
    response += "Connection: close\r\n";
    response += "\r\n";
    
    socket->write(response.toUtf8());
    if (!data.isEmpty()) {
        socket->write(data);
    }
    socket->disconnectFromHost();
}

void CelestronOriginSimulator::processWebSocketCommand(const QString &message) {
    WebSocketConnection *wsConn = qobject_cast<WebSocketConnection*>(sender());
    if (!wsConn) return;

    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject()) {
        return;
    }

    QJsonObject obj = doc.object();

    // Extract common fields
    QString command = obj["Command"].toString();
    QString destination = obj["Destination"].toString();
    QString source = obj["Source"].toString();

    // Log incoming commands
    fprintf(stderr, "\033[32m<<< RECV [%s] %s->%s/%s\033[0m %s\n",
            qPrintable(obj["Type"].toString()), qPrintable(source),
            qPrintable(destination), qPrintable(command),
            qPrintable(QString::fromUtf8(doc.toJson(QJsonDocument::Compact))));
    
    // Handle status requests directly
    if (command == "GetStatus") {
        int sequenceId = obj["SequenceID"].toInt();
        
        if (destination == "System") {
            m_statusSender->sendSystemVersion(wsConn, sequenceId, source);
        } else if (destination == "Mount") {
            m_statusSender->sendMountStatus(wsConn, sequenceId, source);
        } else if (destination == "Focuser") {
            m_statusSender->sendFocuserStatus(wsConn, sequenceId, source);
        } else if (destination == "TaskController") {
            m_statusSender->sendTaskControllerStatus(wsConn, sequenceId, source);
        } else if (destination == "DewHeater") {
            m_statusSender->sendDewHeaterStatus(wsConn, sequenceId, source);
        } else if (destination == "Environment") {
            m_statusSender->sendEnvironmentStatus(wsConn, sequenceId, source);
        } else if (destination == "OrientationSensor") {
            m_statusSender->sendOrientationStatus(wsConn, sequenceId, source);
        } else if (destination == "Disk") {
            m_statusSender->sendDiskStatus(wsConn, sequenceId, source);
        } else if (destination == "FactoryCalibrationController") {
            m_statusSender->sendCalibrationStatus(wsConn, sequenceId, source);
        } else if (destination == "Camera") {
            m_statusSender->sendCameraStatus(wsConn, sequenceId, source);
        } else if (destination == "Autoguider") {
            m_statusSender->sendAutoguiderStatus(wsConn, sequenceId, source);
        } else if (destination == "LedRing" || destination == "LampController") {
            m_statusSender->sendLedRingStatus(wsConn, sequenceId, source);
        } else if (destination == "ElPanel") {
            m_statusSender->sendElPanelStatus(wsConn, sequenceId, source);
        } else if (destination == "Network") {
            m_statusSender->sendNetworkStatus(wsConn, sequenceId, source);
        } else if (destination == "LiveStream") {
            m_statusSender->sendLiveStreamStatus(wsConn, sequenceId, source);
        } else if (destination == "Optics") {
            m_statusSender->sendOpticsStatus(wsConn, sequenceId, source);
        } else if (destination == "HostController") {
            m_statusSender->sendHostControllerStatus(wsConn, sequenceId, source);
        } else if (destination == "Mosaic" || destination == "ImageServer") {
            // Generic OK response for GetStatus to these destinations
            QJsonObject response;
            response["Command"] = "GetStatus";
            response["Destination"] = source;
            response["ErrorCode"] = 0;
            response["ErrorMessage"] = "";
            response["ExpiredAt"] = m_telescopeState->getExpiredAt();
            response["SequenceID"] = sequenceId;
            response["Source"] = destination;
            response["Type"] = "Response";
            QJsonDocument doc(response);
            wsConn->sendTextMessage(doc.toJson(QJsonDocument::Compact));
        }
    } else if (command == "GetVersion") {
        int sequenceId = obj["SequenceID"].toInt();
        m_statusSender->sendSystemVersion(wsConn, sequenceId, source);
    } else if (command == "GetCaptureParameters") {
        int sequenceId = obj["SequenceID"].toInt();
        m_statusSender->sendCameraParams(wsConn, sequenceId, source);
    } else if (command == "GetFilter") {
        int sequenceId = obj["SequenceID"].toInt();
        m_statusSender->sendCameraFilter(wsConn, sequenceId, source);
    } else if (command == "GetModel") {
        int sequenceId = obj["SequenceID"].toInt();
        m_statusSender->sendSystemModel(wsConn, sequenceId, source);
    } else {
        // Process the command through the command handler
        m_commandHandler->processCommand(obj, wsConn);
    }
}

void CelestronOriginSimulator::onWebSocketDisconnected() {
    WebSocketConnection *wsConn = qobject_cast<WebSocketConnection*>(sender());
    if (wsConn) {
        m_webSocketClients.removeAll(wsConn);
        m_statusSender->removeWebSocketClient(wsConn);
        wsConn->deleteLater();
    }
}

// CRITICAL: Fix the status update timing to avoid blocking ping/pong
// Replace sendStatusUpdates method with this non-blocking version:

void CelestronOriginSimulator::sendStatusUpdates() {
    // Update time
    m_telescopeState->dateTime = QDateTime::currentDateTime();
    
    // CRITICAL: Use QueuedConnection to avoid blocking the event loop
    static int updateCounter = 0;
    updateCounter++;
    
    // Send updates in smaller batches to prevent blocking
    if (updateCounter % 1 == 0) {
        // Every second - critical updates only
        QTimer::singleShot(0, this, [this]() {
            m_statusSender->sendMountStatusToAll();
        });
    }
    
    if (updateCounter % 2 == 0) {
        QTimer::singleShot(5, this, [this]() {
            m_statusSender->sendFocuserStatusToAll();
        });
    }
    
    // Camera GetCaptureParameters at ~1 Hz (matches the real device cadence)
    // — the App reads ExposedTime from these to drive the per-exposure
    // progress bar. Decoupled from NewImageReady so the live-preview frame
    // rate stays at its slower cadence.
    QTimer::singleShot(7, this, [this]() {
        m_statusSender->sendCameraParamsToAll();
    });

    if (updateCounter % 3 == 0) {
        QTimer::singleShot(10, this, [this]() {
            m_telescopeState->sequenceNumber++;
            m_telescopeState->fileLocation = m_telescopeState->getNextImageFile();
            m_statusSender->sendNewImageReadyToAll();
        });
    }
    
    // Less frequent updates with longer delays
    if (updateCounter % 10 == 0) {
        QTimer::singleShot(15, this, [this]() {
            m_statusSender->sendEnvironmentStatusToAll();
            m_statusSender->sendDiskStatusToAll();
        });
    }
    
    if (updateCounter % 15 == 0) {
        QTimer::singleShot(20, this, [this]() {
            m_statusSender->sendDewHeaterStatusToAll();
        });
    }
    
    if (updateCounter % 30 == 0) {
        QTimer::singleShot(25, this, [this]() {
            m_statusSender->sendOrientationStatusToAll();
        });
    }
    
    if (updateCounter % 5 == 0) {
        QTimer::singleShot(30, this, [this]() {
            m_statusSender->sendTaskControllerStatusToAll();
        });
    }
    
    // Reset counter to prevent overflow
    if (updateCounter > 1000) {
        updateCounter = 0;
    }
}

// CRITICAL: Add connection monitoring
// Add this method to monitor connection health:

void CelestronOriginSimulator::checkConnectionHealth() {
//     qDebug() << "Active WebSocket connections:" << m_webSocketClients.size();
    
    for (WebSocketConnection *wsConn : m_webSocketClients) {
        // Check if connection is still alive
        // We don't need to do anything here - the ping/pong mechanism handles it
    }
}

// Add these new slot methods to CelestronOriginSimulator class:

void CelestronOriginSimulator::handleWebSocketPing(const QByteArray &payload) {
    WebSocketConnection *wsConn = qobject_cast<WebSocketConnection*>(sender());
//     qDebug() << "WebSocket ping received from client, payload size:" << payload.size();
    // The WebSocketConnection automatically sends pong, we just log it here
}

void CelestronOriginSimulator::handleWebSocketPong(const QByteArray &payload) {
    WebSocketConnection *wsConn = qobject_cast<WebSocketConnection*>(sender());
//     qDebug() << "WebSocket pong received from client, payload size:" << payload.size();
    // Client responded to our ping successfully
}

void CelestronOriginSimulator::handleWebSocketTimeout() {
    WebSocketConnection *wsConn = qobject_cast<WebSocketConnection*>(sender());
//     qDebug() << "WebSocket ping timeout occurred - client not responding";
    
    if (wsConn && m_webSocketClients.contains(wsConn)) {
        // Remove from active clients but don't delete yet - let disconnected signal handle cleanup
        m_statusSender->removeWebSocketClient(wsConn);
    }
}


// Method 1: Save to QByteArray as JPEG/PNG (most common)
QByteArray saveImageToByteArray(const QImage& image, const QString& format = "JPEG", int quality = 95) {
    QByteArray byteArray;
    QBuffer buffer(&byteArray);
    buffer.open(QIODevice::WriteOnly);
    
    // Save image to buffer in specified format
    bool success = image.save(&buffer, format.toUtf8().data(), quality);
    
    if (success) {
        qDebug() << QString("Image saved to buffer: %1 bytes, format: %2")
                    .arg(byteArray.size()).arg(format);
    } else {
        qDebug() << "Failed to save image to buffer";
    }
    
    return byteArray;
}

// And call it in the constructor:
// CelestronOriginSimulator::CelestronOriginSimulator(QObject *parent) : QObject(parent) {
//     // ... existing initialization ...
//     setupHipsIntegration();
//     setupMosaicCreator();  // Add this line
//     // ... rest of initialization ...
// }
