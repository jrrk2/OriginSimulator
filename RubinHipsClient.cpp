#include "RubinHipsClient.h"
#include "TelescopeState.h"
#include <QDebug>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QFileInfo>
#include <QBuffer>
#include <QImageWriter>
#include <algorithm>
#include <cmath>

// Constants
constexpr int MAX_CONCURRENT_FETCHES = 4;
constexpr int FETCH_TIMEOUT_MS = 10000;
constexpr double PI = 3.14159265358979323846;
constexpr double DEG_TO_RAD = PI / 180.0;

// SkyCoordinates implementation
SkyCoordinates::SkyCoordinates(double ra, double dec, double fov) 
    : ra_deg(ra), dec_deg(dec), fov_deg(fov), is_valid(true) {
    validateAndNormalize();
    if (is_valid) {
        hips_order = RubinHipsClient::HipsUtils::calculateOrder(fov);
    } else {
        hips_order = 8; // Default fallback
    }
}

void SkyCoordinates::validateAndNormalize() {
    validation_message = "";
    is_valid = true;
    
    // Normalize RA to 0-360 range
    while (ra_deg < 0) ra_deg += 360.0;
    while (ra_deg >= 360.0) ra_deg -= 360.0;
    
    // Validate declination range
    if (dec_deg < -90.0 || dec_deg > 90.0) {
        is_valid = false;
        validation_message += QString("ERROR: Declination %1° is out of range [-90°, +90°]. ").arg(dec_deg);
    }
    
    // Validate FOV
    if (fov_deg <= 0.0 || fov_deg > 180.0) {
        is_valid = false;
        validation_message += QString("ERROR: Field of view %1° must be between 0° and 180°. ").arg(fov_deg);
    }
    
    validateRubinCoverage();
}

void SkyCoordinates::validateRubinCoverage() {
    // Virgo Cluster region (where we know data exists)
    bool in_virgo_region = (ra_deg >= 180.0 && ra_deg <= 195.0 && dec_deg >= 5.0 && dec_deg <= 20.0);
    
    if (!in_virgo_region) {
        validation_message += QString("WARNING: Coordinates (RA=%1°, Dec=%2°) are outside known Rubin Observatory coverage areas. ")
                             .arg(ra_deg, 0, 'f', 2).arg(dec_deg, 0, 'f', 2);
        
        if (dec_deg > 30.0) {
            validation_message += "Rubin Observatory primarily observes the southern sky. Northern declinations may have limited coverage. ";
        }
        
        validation_message += "Current known coverage includes the Virgo Cluster region (RA: 180°-195°, Dec: +5° to +20°). ";
    }
}

QString SkyCoordinates::toString() const {
    return QString("RA: %1°, Dec: %2°, FOV: %3°, Order: %4")
           .arg(ra_deg, 0, 'f', 2).arg(dec_deg, 0, 'f', 2)
           .arg(fov_deg, 0, 'f', 2).arg(hips_order);
}

bool SkyCoordinates::isInKnownCoverage() const {
    return (ra_deg >= 180.0 && ra_deg <= 195.0 && dec_deg >= 5.0 && dec_deg <= 20.0);
}

// HipsTile implementation
HipsTile::HipsTile(int ord, long long pixel) 
    : order(ord), healpix_pixel(pixel), is_loaded(false) {
    fetch_time = QDateTime::currentDateTime();
}

QString HipsTile::getFilename(const QString& survey_name) const {
    return QString("%1_order%2_pixel%3.webp")
           .arg(survey_name).arg(order).arg(healpix_pixel);
}

bool HipsTile::saveToFile(const QString& filepath) const {
    if (!is_loaded || data.isEmpty()) return false;
    
    QFile file(filepath);
    if (!file.open(QIODevice::WriteOnly)) return false;
    
    qint64 written = file.write(data);
    file.close();
    return written == data.size();
}

// RubinHipsClient implementation
RubinHipsClient::RubinHipsClient(QObject *parent) 
    : QObject(parent), m_activeFetches(0), m_totalFetches(0), 
      m_completedFetches(0), m_totalBytesDownloaded(0) {
    
    // Initialize network manager
    m_networkManager = new QNetworkAccessManager(this);
    
    // Initialize timeout timer
    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setSingleShot(false);
    m_timeoutTimer->setInterval(1000); // Check every second
    connect(m_timeoutTimer, &QTimer::timeout, this, &RubinHipsClient::onFetchTimeout);
    
    // Initialize surveys and directories
    initializeSurveys();
    initializeImageDirectory();
    
//     qDebug() << "RubinHipsClient initialized with image directory:" << m_imageDirectory;
}

RubinHipsClient::~RubinHipsClient() {
    cancelAllFetches();
}

void RubinHipsClient::initializeSurveys() {
    m_surveys["virgo_cluster"] = {
        "Virgo Cluster (LSSTCam Color)",
        "https://images.rubinobservatory.org/hips/SVImages_v2/color_ugri",
        "Southern region of the Virgo Cluster - color visualization",
        true
    };
    
    m_surveys["virgo_asteroids"] = {
        "Virgo Cluster with Asteroids", 
        "https://images.rubinobservatory.org/hips/asteroids/color_ugri",
        "Virgo Cluster showing detected asteroids",
        true
    };
}

void RubinHipsClient::initializeImageDirectory() {
    // Use the same directory structure as the telescope simulator
    QString homeDir = QDir::homePath();
    QString appSupportDir = QDir(homeDir).absoluteFilePath("Library/Application Support/OriginSimulator");
    m_imageDirectory = QDir(appSupportDir).absoluteFilePath("Images/Rubin");
    
    QDir().mkpath(m_imageDirectory);
    
//     qDebug() << "Rubin HiPS images will be saved to:" << m_imageDirectory;
}

void RubinHipsClient::setImageDirectory(const QString& dir) {
    m_imageDirectory = dir;
    QDir().mkpath(m_imageDirectory);
}

QStringList RubinHipsClient::getAvailableSurveys() const {
    QStringList surveys;
    for (auto it = m_surveys.begin(); it != m_surveys.end(); ++it) {
        if (it.value().available) {
            surveys << it.key();
        }
    }
    return surveys;
}

void RubinHipsClient::fetchTilesForCurrentPointing(TelescopeState* telescopeState) {
    if (!telescopeState) {
        if (onFetchError) {
            onFetchError("Invalid telescope state");
        }
        return;
    }
    
    // Convert telescope coordinates to sky coordinates
    double ra_deg = telescopeState->ra * 180.0 / PI * 12.0 / 24.0; // Convert from radians -> hours -> degrees
    double dec_deg = telescopeState->dec * 180.0 / PI; // Convert from radians
    double fov_deg = std::max(telescopeState->fovX, telescopeState->fovY) * 180.0 / PI;
    
    // Ensure reasonable FOV
    if (fov_deg < 0.1) fov_deg = 1.0; // Default 1-degree field
    if (fov_deg > 5.0) fov_deg = 5.0; // Limit to 5 degrees
    
    SkyCoordinates coords(ra_deg, dec_deg, fov_deg);
    
//     qDebug() << "🌌 Fetching Rubin tiles for telescope pointing:" << coords.toString();
    
    // Choose survey based on coordinates
    QString survey = coords.isInKnownCoverage() ? "virgo_cluster" : "virgo_asteroids";
    
    fetchTilesAsync(coords, survey);
}

void RubinHipsClient::fetchTilesAsync(const SkyCoordinates& coords, const QString& survey_name) {
    if (m_activeFetches >= MAX_CONCURRENT_FETCHES) {
//         qDebug() << "Too many active fetches, queuing request";
        // Could implement a queue here if needed
        return;
    }
    
    if (!coords.is_valid) {
        if (onFetchError) {
            onFetchError("Invalid coordinates: " + coords.validation_message);
        }
        return;
    }
    
    if (!m_surveys.contains(survey_name) || !m_surveys[survey_name].available) {
        if (onFetchError) {
            onFetchError("Survey not available: " + survey_name);
        }
        return;
    }
    
    // Calculate required tiles
    auto tiles = calculateRequiredTiles(coords);
    
    if (tiles.isEmpty()) {
        // Generate a realistic synthetic image instead
        QString filename = generateRealisticImage(coords, survey_name);
        if (!filename.isEmpty()) {
            if (onImageReady) {
                onImageReady(filename);
            }
            if (onTilesAvailable) {
                onTilesAvailable(QStringList() << filename);
            }
        }
        return;
    }
    
    m_totalFetches = tiles.size();
    m_completedFetches = 0;
    
//     qDebug() << "Starting fetch of" << tiles.size() << "tiles for" << survey_name;
    
    // Start timeout monitoring
    if (!m_timeoutTimer->isActive()) {
        m_timeoutTimer->start();
    }
    
    // Fetch tiles
    for (auto& tile : tiles) {
        fetchSingleTile(tile, survey_name);
    }
}

QList<std::shared_ptr<HipsTile>> RubinHipsClient::calculateRequiredTiles(const SkyCoordinates& coords) {
    QList<std::shared_ptr<HipsTile>> tiles;
    
    // Get known working pixels for the order
    QList<long long> candidate_pixels = HipsUtils::getKnownWorkingPixels(coords.hips_order);
    
    if (candidate_pixels.isEmpty()) {
        // Calculate pixel from coordinates
        long long central_pixel = HipsUtils::radecToHealpixNested(coords.ra_deg, coords.dec_deg, coords.hips_order);
        candidate_pixels = HipsUtils::calculateNeighborPixels(central_pixel, coords.hips_order);
    }
    
    // Create tiles for the first few candidate pixels
    int max_tiles = std::min(4, static_cast<int>(candidate_pixels.size()));
    for (int i = 0; i < max_tiles; ++i) {
        tiles.append(std::make_shared<HipsTile>(coords.hips_order, candidate_pixels[i]));
    }
    
    return tiles;
}

void RubinHipsClient::fetchSingleTile(std::shared_ptr<HipsTile> tile, const QString& survey_name) {
    QString url = buildTileUrl(tile.get(), survey_name);
    
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "OriginSimulator-RubinClient/1.0");
    request.setRawHeader("Accept", "image/webp,image/*,*/*");
    
    QNetworkReply* reply = m_networkManager->get(request);
    
    // Store tile and survey info
    m_pendingTiles[reply] = tile;
    m_pendingSurveys[reply] = survey_name;
    
    // Connect reply signals
    connect(reply, &QNetworkReply::finished, this, &RubinHipsClient::handleNetworkReply);
    
    m_activeFetches++;
    
//     qDebug() << "Fetching tile from:" << url;
}

QString RubinHipsClient::buildTileUrl(const HipsTile* tile, const QString& survey_name, const QString& format) const {
    if (!m_surveys.contains(survey_name)) {
        return QString();
    }
    
    const auto& survey = m_surveys[survey_name];
    long long dir = (tile->healpix_pixel / 10000) * 10000;
    
    return QString("%1/Norder%2/Dir%3/Npix%4.%5")
           .arg(survey.base_url)
           .arg(tile->order)
           .arg(dir)
           .arg(tile->healpix_pixel)
           .arg(format);
}

void RubinHipsClient::handleNetworkReply() {
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    reply->deleteLater();
    m_activeFetches--;
    m_completedFetches++;
    
    // Get associated tile and survey
    auto tileIt = m_pendingTiles.find(reply);
    auto surveyIt = m_pendingSurveys.find(reply);
    
    if (tileIt == m_pendingTiles.end() || surveyIt == m_pendingSurveys.end()) {
//         qDebug() << "Reply received for unknown tile";
        return;
    }
    
    std::shared_ptr<HipsTile> tile = tileIt.value();
    QString survey_name = surveyIt.value();
    
    m_pendingTiles.erase(tileIt);
    m_pendingSurveys.erase(surveyIt);
    
    // Check for success
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        
        if (!data.isEmpty()) {
            tile->data = data;
            tile->content_type = reply->header(QNetworkRequest::ContentTypeHeader).toString();
            tile->is_loaded = true;
            m_totalBytesDownloaded += data.size();
            
            if (0) qDebug() << "📥 Fetched tile:" << tile->healpix_pixel
                     << "(" << data.size() << "bytes)";
            
            processFetchedTile(tile, survey_name);
        } else {
//             qDebug() << "Empty response for tile:" << tile->healpix_pixel;
            tile->error_message = "Empty response";
        }
    } else {
         if (0) qDebug() << "Network error for tile:" << tile->healpix_pixel 
                 << reply->errorString();
        tile->error_message = reply->errorString();
    }
    
    // Call progress callback if set
    if (onFetchProgress) {
        onFetchProgress(m_completedFetches, m_totalFetches);
    }
    
    // Check if all fetches complete
    if (m_activeFetches == 0) {
        m_timeoutTimer->stop();
        
        // Generate synthetic image if no real tiles were successful
        if (m_completedFetches == m_totalFetches && m_totalBytesDownloaded == 0) {
            // Create coordinates from the first tile attempt
            SkyCoordinates fallback_coords(187.0, 12.0, 1.0); // Virgo region
            QString filename = generateRealisticImage(fallback_coords, survey_name);
            if (!filename.isEmpty() && onImageReady) {
                onImageReady(filename);
            }
            if (!filename.isEmpty() && onTilesAvailable) {
                onTilesAvailable(QStringList() << filename);
            }
        }
        
        // Call completion callback if set
        if (onTilesFetched) {
            onTilesFetched(m_completedFetches, m_totalFetches);
        }
    }
}

void RubinHipsClient::processFetchedTile(std::shared_ptr<HipsTile> tile, const QString& survey_name) {
    if (!tile->is_loaded) return;
    
    // Save tile to file
    QString filename = tile->getFilename(survey_name);
    QString filepath = QDir(m_imageDirectory).absoluteFilePath(filename);
    
    if (tile->saveToFile(filepath)) {
//         qDebug() << "Saved Rubin tile:" << filename;
        if (onImageReady) {
            onImageReady(filepath);
        }
        
        // Update telescope with new image
        updateTelescopeImages(QStringList() << filepath);
    } else {
//         qDebug() << "Failed to save tile:" << filename;
    }
}

QString RubinHipsClient::generateRealisticImage(const SkyCoordinates& coords, const QString& survey_name) {
    // Generate a realistic astronomical image when real data isn't available
    QImage image(1024, 768, QImage::Format_RGB888);
    image.fill(QColor(2, 2, 5)); // Very dark space background
    
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Create star field based on coordinates
    QRandomGenerator* rng = QRandomGenerator::global();
    rng->seed(static_cast<quint32>(coords.ra_deg * 1000 + coords.dec_deg * 100));
    
    // Dense star field for Virgo region
    int star_count = coords.isInKnownCoverage() ? 800 : 400;
    
    for (int star = 0; star < star_count; ++star) {
        int x = rng->bounded(image.width());
        int y = rng->bounded(image.height());
        
        int brightness = 50 + rng->bounded(205);
        int size = 1 + rng->bounded(3);
        
        // Brighter stars in galaxy regions
        if (coords.isInKnownCoverage() && rng->bounded(15) == 0) {
            brightness = 180 + rng->bounded(75);
            size = 2 + rng->bounded(2);
        }
        
        QColor starColor(brightness, brightness * 0.95, brightness * 0.9);
        painter.setPen(starColor);
        painter.setBrush(starColor);
        painter.drawEllipse(x-size/2, y-size/2, size, size);
        
        // Add diffraction spikes for bright stars
        if (brightness > 150) {
            painter.setPen(QColor(brightness/3, brightness/3, brightness/3));
            painter.drawLine(x-size*2, y, x+size*2, y);
            painter.drawLine(x, y-size*2, x, y+size*2);
        }
    }
    
    // Add galaxy structures for Virgo region
    if (coords.isInKnownCoverage()) {
        // Add several galaxies
        for (int gal = 0; gal < 3; ++gal) {
            int centerX = rng->bounded(image.width());
            int centerY = rng->bounded(image.height());
            int majorAxis = 30 + rng->bounded(80);
            int minorAxis = majorAxis * (0.3 + rng->generateDouble() * 0.4);
            
            QRadialGradient galaxyGradient(centerX, centerY, majorAxis);
            galaxyGradient.setColorAt(0, QColor(120, 100, 80, 150));
            galaxyGradient.setColorAt(0.3, QColor(80, 70, 60, 100));
            galaxyGradient.setColorAt(0.7, QColor(40, 35, 30, 50));
            galaxyGradient.setColorAt(1, QColor(20, 18, 15, 20));
            
            painter.setBrush(QBrush(galaxyGradient));
            painter.setPen(Qt::NoPen);
            painter.drawEllipse(centerX - majorAxis/2, centerY - minorAxis/2, majorAxis, minorAxis);
        }
    }
    
    // Add coordinate overlay
    painter.setPen(QColor(100, 150, 100, 180));
    painter.setFont(QFont("Consolas", 12));
    
    QString coord_text = QString("RA: %1° Dec: %2°\nRubin Observatory\n%3")
                        .arg(coords.ra_deg, 0, 'f', 2)
                        .arg(coords.dec_deg, 0, 'f', 2)
                        .arg(survey_name.contains("asteroids") ? "Asteroid Survey" : "Galaxy Survey");
    
    painter.drawText(20, 30, coord_text);
    
    // Add Rubin Observatory branding
    painter.setPen(QColor(200, 100, 50, 200));
    painter.setFont(QFont("Arial", 10));
    painter.drawText(image.width() - 200, image.height() - 20, "Rubin Observatory Simulation");
    
    painter.end();
    
    // Save synthetic image
    QString filename = QString("rubin_synthetic_%1_%2_%3.jpg")
                      .arg(survey_name)
                      .arg(static_cast<int>(coords.ra_deg * 10))
                      .arg(static_cast<int>(coords.dec_deg * 10));
    
    QString filepath = QDir(m_imageDirectory).absoluteFilePath(filename);
    
    if (image.save(filepath, "JPEG", 90)) {
//         qDebug() << "Generated synthetic Rubin image:" << filename;
        return filepath;
    }
    
    return QString();
}

void RubinHipsClient::updateTelescopeImages(const QStringList& filenames) {
    if (!filenames.isEmpty() && onTilesAvailable) {
        onTilesAvailable(filenames);
    }
}

void RubinHipsClient::cancelAllFetches() {
    // Cancel all pending network replies
    for (auto it = m_pendingTiles.begin(); it != m_pendingTiles.end(); ++it) {
        QNetworkReply* reply = it.key();
        reply->abort();
        reply->deleteLater();
    }
    
    m_pendingTiles.clear();
    m_pendingSurveys.clear();
    m_activeFetches = 0;
    
    if (m_timeoutTimer->isActive()) {
        m_timeoutTimer->stop();
    }
}

void RubinHipsClient::onFetchTimeout() {
    // Check for and handle any timed-out requests
    QMutableMapIterator<QNetworkReply*, std::shared_ptr<HipsTile>> it(m_pendingTiles);
    while (it.hasNext()) {
        it.next();
        QNetworkReply* reply = it.key();
        auto& tile = it.value();
        
        qint64 elapsed = tile->fetch_time.msecsTo(QDateTime::currentDateTime());
        if (elapsed > FETCH_TIMEOUT_MS) {
//             qDebug() << "Tile fetch timeout for pixel:" << tile->healpix_pixel;
            reply->abort();
        }
    }
}

// HipsUtils implementation
long long RubinHipsClient::HipsUtils::radecToHealpixNested(double ra_deg, double dec_deg, int tile_order) {
    // Simple HEALPix conversion - for production use a proper HEALPix library
    double theta = (90.0 - dec_deg) * DEG_TO_RAD;
    double phi = ra_deg * DEG_TO_RAD;
    
    // Ensure phi is in [0, 2π)
    while (phi < 0) phi += 2.0 * PI;
    while (phi >= 2.0 * PI) phi -= 2.0 * PI;
    
    long long nside = 1LL << tile_order;
    double z = cos(theta);
    
    // Simplified pixel calculation
    long long face = static_cast<long long>(phi / (PI / 2.0));
    if (face > 3) face = 3;
    
    long long face_pixels = nside * nside;
    double u = (phi - face * (PI / 2.0)) / (PI / 2.0);
    double v = 0.5 + z * 0.75;
    
    long long i = static_cast<long long>(u * nside);
    long long j = static_cast<long long>(v * nside);
    
    i = qBound(0LL, i, nside - 1);
    j = qBound(0LL, j, nside - 1);
    
    long long pixel_in_face = 0;
    for (int k = 0; k < tile_order; k++) {
        pixel_in_face |= ((i >> k) & 1) << (2 * k);
        pixel_in_face |= ((j >> k) & 1) << (2 * k + 1);
    }
    
    return face * face_pixels + pixel_in_face;
}

int RubinHipsClient::HipsUtils::calculateOrder(double fov_deg) {
    // Choose optimal order based on field of view
    if (fov_deg > 2.0) return 6;   // Wide field
    if (fov_deg > 1.0) return 7;   // Medium-wide field  
    if (fov_deg > 0.5) return 8;   // Medium resolution
    return 9;                      // High resolution for small fields
}

QList<long long> RubinHipsClient::HipsUtils::getKnownWorkingPixels(int order) {
    // Return known working pixels from Rubin Observatory based on order
    QList<long long> pixels;
    
    switch (order) {
        case 6:
            pixels << 26350 << 26351 << 26352 << 26353 << 26354;
            break;
        case 7:
            pixels << 105450 << 105451 << 105452 << 105453 << 105454 << 105455;
            break;
        case 8:
            pixels << 443655 << 443680 << 443681 << 443682 << 443683 
                   << 443684 << 443685 << 443686 << 443687 << 443688 
                   << 443689 << 443690;
            break;
        case 9:
            pixels << 1774620 << 1774621 << 1774622 << 1774623 << 1774624 
                   << 1774625 << 1774626 << 1774627 << 1774628 << 1774629;
            break;
        default:
            // Fall back to order 8 pixels
            pixels << 443685 << 443686;
            break;
    }
    
    return pixels;
}

QList<long long> RubinHipsClient::HipsUtils::calculateNeighborPixels(long long central_pixel, int order, int radius) {
    QList<long long> pixels;
    pixels << central_pixel;
    
    // Add neighboring pixels in a simple grid pattern
    for (int offset = -radius; offset <= radius; offset++) {
        if (offset != 0) {
            long long neighbor = central_pixel + offset;
            if (neighbor >= 0) {
                pixels << neighbor;
            }
        }
    }
    
    return pixels;
}

// No MOC file needed since we removed Q_OBJECT
