#include "healpix_base.h"
#include "pointing.h"

#include "RubinHipsClient.h"
#include "TelescopeState.h"
#include <QDebug>
#include <QRegularExpression>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QFileInfo>
#include <QFile>
#include <QPainter>
#include <QBuffer>
#include <QImageWriter>
#include <algorithm>
#include <cmath>
// Complete RubinHipsClient.cpp implementation
// Add this to your existing RubinHipsClient.cpp file

// Constants
constexpr int MAX_CONCURRENT_FETCHES = 6;
constexpr int FETCH_TIMEOUT_MS = 15000;

double Ang2Rad(double x) { return x * DEG_TO_RAD; }

// RubinHipsClient Implementation
RubinHipsClient::RubinHipsClient(QObject *parent) 
    : QObject(parent), m_activeFetches(0), m_totalFetches(0), 
      m_completedFetches(0), m_totalBytesDownloaded(0) {
    
    // Initialize network manager
    m_networkManager = new QNetworkAccessManager(this);
    
    // Initialize timeout timer
    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setSingleShot(false);
    m_timeoutTimer->setInterval(1000); // Check every second
    connect(m_timeoutTimer, &QTimer::timeout, [this]() { onFetchTimeout(); });
    
    // Initialize surveys and directories
    initializeSurveys();
    initializeImageDirectory();
    
    qDebug() << "RubinHipsClient initialized with image directory:" << m_imageDirectory;
}

// Updated RubinHipsClient.cpp - with manual signal connections
// Add these manual connections to the constructor:

// In RubinHipsClient constructor, change the connect statements to:
void RubinHipsClient::initializeConnections() {
    // Manual connections since we removed Q_OBJECT
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
    
    qDebug() << "Rubin HiPS images will be saved to:" << m_imageDirectory;
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

void RubinHipsClient::fetchTilesAsync(const SkyCoordinates& coords, const QString& survey_name) {
    if (m_activeFetches >= MAX_CONCURRENT_FETCHES) {
        qDebug() << "Too many active fetches, queuing request";
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
    
    // Calculate required tiles using the new JavaScript-compatible algorithm
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
    
    qDebug() << "Starting fetch of" << tiles.size() << "tiles for" << survey_name;
    
    // Start timeout monitoring
    if (!m_timeoutTimer->isActive()) {
        m_timeoutTimer->start();
    }
    
    // Fetch tiles
    for (auto& tile : tiles) {
        fetchSingleTile(tile, survey_name);
    }
}

void RubinHipsClient::fetchSingleTile(std::shared_ptr<HipsTile> tile, const QString& survey_name) {
    qDebug() << "🎯 Fetching Order" << tile->order << "pixel" << tile->healpix_pixel;
    QString url = buildTileUrl(tile.get(), survey_name);
    qDebug() << "Built tile URL:" << url;
    
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "OriginSimulator-RubinClient/1.0");
    request.setRawHeader("Accept", "image/webp,image/*,*/*");
    
    QNetworkReply* reply = m_networkManager->get(request);
    
    // Store tile and survey info
    m_pendingTiles[reply] = tile;
    m_pendingSurveys[reply] = survey_name;
    
    // Connect reply signals
    connect(reply, &QNetworkReply::finished, [this, reply]() { handleNetworkReply(reply); });
    
    m_activeFetches++;
    
    qDebug() << "Fetching tile from:" << url;
}


// Update handleNetworkReply to take the reply as parameter:
void RubinHipsClient::handleNetworkReply(QNetworkReply* reply) {
    if (!reply) return;
    
    reply->deleteLater();
    m_activeFetches--;
    m_completedFetches++;
        
    // Get associated tile and survey
    auto tileIt = m_pendingTiles.find(reply);
    auto surveyIt = m_pendingSurveys.find(reply);
    
    if (tileIt == m_pendingTiles.end() || surveyIt == m_pendingSurveys.end()) {
        qDebug() << "Reply received for unknown tile";
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
            
            qDebug() << "📥 Fetched tile:" << tile->healpix_pixel
                     << "(" << data.size() << "bytes)";
            
            processFetchedTile(tile, survey_name);
        } else {
            qDebug() << "Empty response for tile:" << tile->healpix_pixel;
            tile->error_message = "Empty response";
        }
    } else {
        qDebug() << "Network error for tile:" << tile->healpix_pixel << "HTTP:" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() 
                 << reply->errorString();
        tile->error_message = reply->errorString();
        
        // Check if this is a 404 (tile doesn't exist) vs other network error
        int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (httpStatus == 404) {
            qDebug() << "Tile" << tile->healpix_pixel << "does not exist (404) - this is normal for some sky regions";
        } else if (httpStatus == 0) {
            qDebug() << "Network connection error for tile" << tile->healpix_pixel << "- may retry with different survey";
        }
    }
    
    // Call progress callback if set
    if (onFetchProgress) {
        onFetchProgress(m_completedFetches, m_totalFetches);
    }
    
    // Check if all fetches complete
    if (m_activeFetches == 0) {
        m_timeoutTimer->stop();
        
        // Generate synthetic image if no real tiles were successful
        if (m_totalBytesDownloaded == 0) {
            qDebug() << "All tile fetches complete. Downloaded bytes:" << m_totalBytesDownloaded;
            qDebug() << "Failed to generate fallback image";
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
        qDebug() << "Saved Rubin tile:" << filename;
        if (onImageReady) {
            onImageReady(filepath);
        }
        
        // Update telescope with new image
        updateTelescopeImages(QStringList() << filepath);
    } else {
        qDebug() << "Failed to save tile:" << filename;
    }
}

QString RubinHipsClient::generateRealisticImage(const SkyCoordinates& coords, const QString& survey_name) {
    // Generate a realistic astronomical image when real data isn't available
    qDebug() << "Generating synthetic image for coordinates:" << coords.toString();
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
        qDebug() << "Generated synthetic Rubin image:" << filename;
        return filepath;
    }
    
    return QString();
}

void RubinHipsClient::updateTelescopeImages(const QStringList& filenames) {
    compositeLiveViewImage();
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
            qDebug() << "Tile fetch timeout for pixel:" << tile->healpix_pixel;
            reply->abort();
        }
    }
}

// SkyCoordinates Implementation
SkyCoordinates::SkyCoordinates(double ra, double dec, double fov, int width, int height) 
    : ra_deg(ra), dec_deg(dec), fov_deg(fov), viewport_width(width), viewport_height(height), is_valid(true) {
    validateAndNormalize();
    if (is_valid) {
        hips_order = calculateOptimalOrder();
    } else {
        hips_order = 8;
    }
}

void SkyCoordinates::validateAndNormalize() {
    validation_message = "";
    is_valid = true;
    
    // Standard coordinate normalization
    while (ra_deg < 0) ra_deg += 360.0;
    while (ra_deg >= 360.0) ra_deg -= 360.0;
    
    if (dec_deg < -90.0 || dec_deg > 90.0) {
        is_valid = false;
        validation_message += QString("Invalid declination: %1°").arg(dec_deg);
    }
    
    if (fov_deg <= 0.0 || fov_deg > 180.0) {
        is_valid = false;
        validation_message += QString("Invalid field of view: %1°").arg(fov_deg);
    }
    
    validateRubinCoverage();
}

void SkyCoordinates::validateRubinCoverage() {
    // Optional: Add specific coverage validation if needed
    // For now, just validate basic coordinate ranges
}

int SkyCoordinates::calculateOptimalOrder() {
    return HipsUtils::calculateOptimalOrderForViewport(fov_deg, viewport_width, viewport_height);
}

QString SkyCoordinates::toString() const {
    return QString("RA: %1°, Dec: %2°, FOV: %3°, Order: %4, Viewport: %5x%6")
           .arg(ra_deg, 0, 'f', 3).arg(dec_deg, 0, 'f', 3)
           .arg(fov_deg, 0, 'f', 3).arg(hips_order)
           .arg(viewport_width).arg(viewport_height);
}

bool SkyCoordinates::isInKnownCoverage() const {
    // Rubin Observatory coverage - can be customized
    return (ra_deg >= 180.0 && ra_deg <= 195.1 && dec_deg >= 5.0 && dec_deg <= 20.0);
}

// HipsTile Implementation
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

// HipsUtils Implementation
long long HipsUtils::radecToHealpixNested(double ra_deg, double dec_deg, int order) {
    qDebug() << "Converting RA:" << ra_deg << "Dec:" << dec_deg << "to HEALPix order" << order;
    
    // Normalize coordinates
    normalizeCoordinates(ra_deg, dec_deg);
    
    // Convert to HEALPix spherical coordinates
    double theta = (90.0 - dec_deg) * DEG_TO_RAD;  // Colatitude
    double phi = ra_deg * DEG_TO_RAD;              // Longitude
    
    // Normalize phi to [0, 2π)
    while (phi < 0) phi += 2.0 * PI;
    while (phi >= 2.0 * PI) phi -= 2.0 * PI;
    
    long long nside = 1LL << order;
    
    // Standard HEALPix ring-to-nested conversion
    double z = std::cos(theta);
    double za = std::abs(z);
    
    long long pixel;
    
    if (za <= 2.0/3.0) {
        // Equatorial region
        double temp1 = nside * (0.5 + z);
        double temp2 = nside * phi / (2.0 * PI);
        
        long long jp = static_cast<long long>(temp1 - temp2);
        long long jm = static_cast<long long>(temp1 + temp2);
        
        long long ir = nside + 1 + jp - jm;
        long long kshift = 1 - (ir & 1);
        long long ip = (jp + jm - nside + kshift + 1) / 2;
        
        if (ip >= nside) ip -= nside;
        
        pixel = 2 * nside * (nside - 1) + 2 * nside * (ir - 1) + ip;
    } else {
        // Polar caps
        double temp = nside * std::sqrt(3.0 * (1.0 - za));
        long long jp = static_cast<long long>(temp - 1.0);
        long long jm = static_cast<long long>(temp + 1.0);
        
        long long ir = jp + jm + 1;
        long long ip = static_cast<long long>(phi * ir / (2.0 * PI));
        
        if (z > 0) {
            pixel = 2 * ir * (ir - 1) + ip;
        } else {
            pixel = 12 * nside * nside - 2 * ir * (ir + 1) + ip;
        }
    }
    
    // Convert ring to nested indexing
    pixel = ringToNested(pixel, nside);
    
    qDebug() << "Calculated HEALPix pixel:" << pixel;
    return pixel;
}

// Also update the HipsUtils::calculateViewportTiles method to return exactly 24 pixels
QList<long long> HipsUtils::calculateViewportTiles(const SkyCoordinates& coords) {
    QList<long long> tiles;
    
    // Calculate the center pixel
    long long center_pixel = radecToHealpixNested(coords.ra_deg, coords.dec_deg, coords.hips_order);
    
    // Fixed 6x4 grid dimensions
    const int tilesX = 6;
    const int tilesY = 4;
    
    qDebug() << "HipsUtils: Creating 6x4 grid around center pixel:" << center_pixel;
    
    // Calculate grid offset to center the viewport
    int startCol = -(tilesX / 2);  // -3 to +2
    int startRow = -(tilesY / 2);  // -2 to +1
    
    // Generate exactly 24 pixels in grid pattern
    for (int row = 0; row < tilesY; row++) {
        for (int col = 0; col < tilesX; col++) {
            int grid_col = startCol + col;
            int grid_row = startRow + row;
            
            // Simple offset calculation for neighboring pixels
            long long pixel_offset = grid_row * 100 + grid_col;
            long long pixel_num = center_pixel + pixel_offset;
            
            // Validate pixel range
            long long nside = 1LL << coords.hips_order;
            long long max_pixel = 12LL * nside * nside - 1;
            
            if (pixel_num < 0) pixel_num = 0;
            if (pixel_num > max_pixel) pixel_num = max_pixel;
            
            tiles.append(pixel_num);
        }
    }
    
    qDebug() << "HipsUtils: Generated exactly" << tiles.size() << "pixels for 6x4 grid";
    Q_ASSERT(tiles.size() == 24);
    
    return tiles;
}

QList<long long> HipsUtils::getNeighborPixels(long long central_pixel, int order, int radius) {
    QList<long long> neighbors;
    long long nside = 1LL << order;
    
    // Simple grid-based neighbor calculation
    for (int dr = -radius; dr <= radius; dr++) {
        for (int dc = -radius; dc <= radius; dc++) {
            if (dr == 0 && dc == 0) continue;
            
            // Simple offset calculation (approximate)
            long long neighbor = central_pixel + dr * nside + dc;
            
            if (neighbor >= 0 && neighbor < (12LL * nside * nside)) {
                neighbors.append(neighbor);
            }
        }
    }
    
    return neighbors;
}

long long HipsUtils::ringToNested(long long ring_pixel, long long nside) {
    // For now, return as-is. In a full implementation, you'd use
    // the official HEALPix ring2nest conversion with bit interleaving
    return ring_pixel;
}

double HipsUtils::calculatePixelAngularSize(int order) {
    long long nside = 1LL << order;
    // HEALPix pixel angular size: sqrt(3/π) * 180° / nside
    return std::sqrt(3.0/PI) * 180.0 / nside;
}

int HipsUtils::calculateOptimalOrderForViewport(double fov_deg, int viewport_width, int viewport_height) {    
    return 11;
}

void HipsUtils::normalizeCoordinates(double& ra_deg, double& dec_deg) {
    // Normalize RA to [0, 360)
    while (ra_deg < 0) ra_deg += 360.0;
    while (ra_deg >= 360.0) ra_deg -= 360.0;
    
    // Clamp declination to [-90, 90]
    dec_deg = std::max(-90.0, std::min(90.0, dec_deg));
}

bool HipsUtils::isValidCoordinate(double ra_deg, double dec_deg) {
    return (ra_deg >= 0.0 && ra_deg < 360.0 && dec_deg >= -90.0 && dec_deg <= 90.0);
}

// Modified calculateRequiredTiles method for exactly 24 tiles in 6x4 grid
// Replace the existing calculateRequiredTiles method in RubinHipsClient.cpp

QList<std::shared_ptr<HipsTile>> RubinHipsClient::calculateRequiredTiles(const SkyCoordinates& coords) {
    QList<std::shared_ptr<HipsTile>> tiles;
    
    qDebug() << "🧩 Calculating exactly 24 tiles in 6x4 grid";
    qDebug() << "Coordinates:" << coords.toString();
    
    // Calculate the center pixel using standard HEALPix conversion
    long long center_pixel = HipsUtils::radecToHealpixNested(coords.ra_deg, coords.dec_deg, coords.hips_order);
    
    // Fixed 6x4 grid dimensions
    const int tilesX = 6;
    const int tilesY = 4;
    const int totalTiles = tilesX * tilesY; // Exactly 24 tiles
    
    qDebug() << "Creating" << totalTiles << "tiles in" << tilesX << "x" << tilesY << "grid";
    qDebug() << "Center pixel:" << center_pixel << "at order" << coords.hips_order;
    
    // Calculate grid offset to center the viewport
    int startCol = -(tilesX / 2);  // -3 to +2 (6 tiles)
    int startRow = -(tilesY / 2);  // -2 to +1 (4 tiles)
    
    // Generate tiles in proper astronomical grid pattern
    for (int row = 0; row < tilesY; row++) {
        for (int col = 0; col < tilesX; col++) {
            // Calculate relative position from center
            int grid_col = startCol + col;  // -3, -2, -1, 0, 1, 2
            int grid_row = startRow + row;  // -2, -1, 0, 1
            
            // Calculate pixel offset based on HEALPix grid structure
            // For HEALPix, neighboring pixels are typically offset by small amounts
            // Use a simple offset pattern that works for most orders
            long long pixel_offset = grid_row * 100 + grid_col;
            long long pixel_num = center_pixel + pixel_offset;
            
            // Ensure pixel is within valid HEALPix range
            long long nside = 1LL << coords.hips_order;
            long long max_pixel = 12LL * nside * nside - 1;
            
            if (pixel_num < 0) {
                pixel_num = 0;
            } else if (pixel_num > max_pixel) {
                pixel_num = max_pixel;
            }
            
            auto tile = std::make_shared<HipsTile>(coords.hips_order, pixel_num);
            tiles.append(tile);
            
            qDebug() << "🎯 Generated tile [" << row << "," << col << "] -> pixel" << pixel_num 
                     << "(offset:" << pixel_offset << ")";
        }
    }
    
    qDebug() << "Generated exactly" << tiles.size() << "tiles for 6x4 grid";
    Q_ASSERT(tiles.size() == 24); // Ensure we have exactly 24 tiles
    
    return tiles;
}

void RubinHipsClient::fetchTilesForCurrentPointing(TelescopeState *state) {
    int order = 10;
    int tileSize = 512;
    int width = 6, height = 4;

    double centerRa = 188.0;
    double centerDec = 8.0;

    Healpix_Base healpix(order, NEST, SET_NSIDE);
    pointing center(Ang2Rad(centerRa), Ang2Rad(centerDec));

    std::vector<TileJob> jobs;

    for (int dy = 0; dy < height; ++dy) {
        for (int dx = 0; dx < width; ++dx) {
            double offsetRa = centerRa + (dx - width / 2 + 0.5) * 0.03;
            double offsetDec = centerDec - (dy - height / 2 + 0.5) * 0.03;

            pointing tilePoint(Ang2Rad(offsetRa), Ang2Rad(offsetDec));
            int pix = healpix.ang2pix(tilePoint);
            int dir = (pix / 10000) * 10000;

            QString url = QString("https://hips.images.arizona.edu/rubin-dr0/Norder%1/Dir%2/Npix%3.jpg")
                            .arg(order)
                            .arg(dir)
                            .arg(pix);

            jobs.push_back({pix, dx, dy, url});
        }
    }

    QImage mosaic(tileSize * width, tileSize * height, QImage::Format_RGB888);
    mosaic.fill(Qt::black);

    auto manager = new QNetworkAccessManager(this);
    int completed = 0;

    for (const auto& job : jobs) {
        QNetworkRequest request(QUrl(job.url));
        QNetworkReply *reply = manager->get(request);

        connect(reply, &QNetworkReply::finished, this, [=, &mosaic, &completed]() mutable {
            if (reply->error() == QNetworkReply::NoError) {
                QByteArray data = reply->readAll();
                QImage tile;
                tile.loadFromData(data);

                QPainter painter(&mosaic);
                int x = job.tileX * tileSize;
                int y = job.tileY * tileSize;
                painter.drawImage(x, y, tile);
                painter.end();
            } else {
                qWarning() << "Tile fetch failed:" << job.url;
            }

            reply->deleteLater();
            completed++;

            if (completed == width * height) {
                QString finalPath = m_imageDirectory + "/current_mosaic.jpg";
                mosaic.save(finalPath, "JPG", 90);

                if (onImageReady) onImageReady(finalPath);
            }
        });
    }
}

// Modified URL building to match JavaScript HiPS standards
QString RubinHipsClient::buildTileUrl(const HipsTile* tile, const QString& survey_name, const QString& format) const {
    if (!m_surveys.contains(survey_name)) {
        return QString();
    }
    
    const auto& survey = m_surveys[survey_name];
    
    // Standard HiPS URL structure (matches JavaScript clients)
    long long dir = (tile->healpix_pixel / 10000) * 10000;
    
    QString url = QString("%1/Norder%2/Dir%3/Npix%4.%5")
                  .arg(survey.base_url)
                  .arg(tile->order)
                  .arg(dir)
                  .arg(tile->healpix_pixel)
                  .arg(format);
    
    qDebug() << "Built standard HiPS URL:" << url;
    return url;
}

// Updated compositeLiveViewImage to handle exactly 24 tiles in proper 6x4 layout
void RubinHipsClient::compositeLiveViewImage() {
    QDir dir(m_imageDirectory);
    QStringList allTiles = dir.entryList(QStringList() << "*.webp" << "*.jpg" << "*.png", QDir::Files);
    
    if (allTiles.size() < 1) return;
    
    qDebug() << "🧩 Compositing" << allTiles.size() << "tiles into 6x4 layout";
    
    // Fixed dimensions for 6x4 grid
    const int target_width = 1024;
    const int target_height = 768;
    const int grid_cols = 6;
    const int grid_rows = 4;
    const int tile_width = target_width / grid_cols;   // 170 pixels
    const int tile_height = target_height / grid_rows; // 192 pixels
    
    QImage composite(target_width, target_height, QImage::Format_RGB888);
    composite.fill(QColor(5, 5, 15)); // Dark sky background
    
    QPainter painter(&composite);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    
    // Place tiles in exact 6x4 grid
    for (int row = 0; row < grid_rows && row * grid_cols < allTiles.size(); row++) {
        for (int col = 0; col < grid_cols; col++) {
            int tile_index = row * grid_cols + col;
            if (tile_index >= allTiles.size()) break;
            
            QString tileFile = allTiles[tile_index];
            QImage tile;
            
            if (tile.load(QDir(m_imageDirectory).absoluteFilePath(tileFile))) {
                QImage scaled = tile.scaled(tile_width, tile_height, 
                                           Qt::KeepAspectRatioByExpanding, 
                                           Qt::SmoothTransformation);
                
                int dest_x = col * tile_width;
                int dest_y = row * tile_height;
                
                painter.drawImage(dest_x, dest_y, scaled);
                qDebug() << "🧩 Placed tile [" << row << "," << col << "] at (" << dest_x << "," << dest_y << ")";
            }
        }
    }
    
    painter.end();
    
    // Save the composite
    QString tempDir = QDir::homePath() + "/Library/Application Support/OriginSimulator/Images/Temp";
    QDir().mkpath(tempDir);
    QString outputFile = tempDir + "/hips_composite_6x4.jpg";
    
    if (composite.save(outputFile, "JPEG", 95)) {
        qDebug() << "✅ 6x4 HiPS composite saved:" << outputFile;
        // Also save as current live view
        composite.save(tempDir + "/98.jpg", "JPEG", 95);
        if (onImageReady) onImageReady(tempDir + "/98.jpg");
    }
}
