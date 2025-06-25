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

// Add these helper functions to validate coordinates
bool isValidTheta(double theta) {
    return theta >= 0.0 && theta <= M_PI;
}

bool isValidPhi(double phi) {
    return phi >= 0.0 && phi < 2.0 * M_PI;
}

void validateAndClampCoordinates(double& theta, double& phi) {
    // Clamp theta to [0, π]
    if (theta < 0.0) theta = 0.0;
    if (theta > M_PI) theta = M_PI;
    
    // Normalize phi to [0, 2π)
    while (phi < 0.0) phi += 2.0 * M_PI;
    while (phi >= 2.0 * M_PI) phi -= 2.0 * M_PI;
}

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

// Fixed HipsUtils::radecToHealpixNested function
long long HipsUtils::radecToHealpixNested(double ra_deg, double dec_deg, int order) {
    qDebug() << "Converting RA:" << ra_deg << "° Dec:" << dec_deg << "° to HEALPix order" << order;
    
    // Validate input coordinates
    if (dec_deg < -90.0 || dec_deg > 90.0) {
        qWarning() << "Invalid declination:" << dec_deg;
        return -1;
    }
    
    // Normalize RA to [0, 360)
    while (ra_deg < 0.0) ra_deg += 360.0;
    while (ra_deg >= 360.0) ra_deg -= 360.0;
    
    // Convert to HEALPix spherical coordinates
    double theta = (90.0 - dec_deg) * M_PI / 180.0;  // colatitude in radians [0, π]
    double phi = ra_deg * M_PI / 180.0;              // longitude in radians [0, 2π]
    
    qDebug() << "Converted to theta:" << theta << "phi:" << phi;
    qDebug() << "Theta in degrees:" << (theta * 180.0 / M_PI) << "Phi in degrees:" << (phi * 180.0 / M_PI);
    
    // Validate converted coordinates
    if (!isValidTheta(theta) || !isValidPhi(phi)) {
        qWarning() << "Invalid spherical coordinates - theta:" << theta << "phi:" << phi;
        return -1;
    }
    
    long long nside = 1LL << order;  // nside = 2^order
    
    try {
        Healpix_Base healpix(nside, NEST, SET_NSIDE);
        pointing pt(theta, phi);
        long long pixel = healpix.ang2pix(pt);
        
        qDebug() << "✓ HEALPix pixel:" << pixel << "using nside:" << nside;
        return pixel;
    } catch (const std::exception& e) {
        qDebug() << "✗ HEALPix calculation error:" << e.what();
        return -1;
    }
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

// Add this helper method to generate a synthetic image when HEALPix fails
void RubinHipsClient::generateSyntheticImage(double ra_deg, double dec_deg) {
    qDebug() << "Generating synthetic image for RA:" << ra_deg << "Dec:" << dec_deg;
    
    QImage synthetic(1024, 768, QImage::Format_RGB888);
    synthetic.fill(QColor(5, 5, 15)); // Dark sky
    
    QPainter painter(&synthetic);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Add coordinate info
    painter.setPen(QColor(100, 150, 200, 200));
    painter.setFont(QFont("Arial", 14));
    QString coordText = QString("RA: %1° Dec: %2°\nRubin Observatory\nSynthetic Field")
                       .arg(ra_deg, 0, 'f', 2)
                       .arg(dec_deg, 0, 'f', 2);
    painter.drawText(20, 30, coordText);
    
    // Add some stars
    QRandomGenerator* rng = QRandomGenerator::global();
    rng->seed(static_cast<quint32>(ra_deg * 1000 + dec_deg * 100));
    
    for (int i = 0; i < 300; ++i) {
        int x = rng->bounded(synthetic.width());
        int y = rng->bounded(synthetic.height());
        int brightness = 50 + rng->bounded(150);
        
        painter.setPen(QColor(brightness, brightness, brightness));
        painter.drawPoint(x, y);
    }
    
    painter.end();
    
    QString syntheticPath = m_imageDirectory + "/synthetic_field.jpg";
    if (synthetic.save(syntheticPath, "JPG", 90)) {
        qDebug() << "✓ Generated synthetic image:" << syntheticPath;
        if (onImageReady) onImageReady(syntheticPath);
    }
}
// Enhanced RubinHipsClient with multiple survey fallbacks and proper 404 handling

void RubinHipsClient::initializeSurveys() {
    // Add multiple survey options with better sky coverage
    m_surveys["rubin_dr0"] = {
        "Rubin DR0 Survey",
        "https://hips.images.arizona.edu/rubin-dr0",
        "Rubin Observatory Data Release 0",
        true
    };
    
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
    
    // Add DSS (Digitized Sky Survey) as reliable fallback
    m_surveys["dss2_color"] = {
        "DSS2 Color",
        "http://alasky.u-strasbg.fr/DSS/DSSColor",
        "Digitized Sky Survey 2 Color - full sky coverage",
        true
    };
    
    m_surveys["2mass"] = {
        "2MASS All-Sky Survey",
        "http://alasky.u-strasbg.fr/2MASS/Color",
        "2MASS infrared all-sky survey",
        true
    };
}

// Enhanced fetchTilesForCurrentPointing with multiple survey fallback
void RubinHipsClient::fetchTilesForCurrentPointing(TelescopeState *state) {
    int order = 10;  // Back to order 10 since working URL uses this
    int nside = 1 << order;
    int tileSize = 512;
    int width = 6, height = 4;

    // Convert telescope coordinates from radians to degrees
    double centerRa = state->ra * 180.0 / M_PI;
    double centerDec = state->dec * 180.0 / M_PI;
    
    qDebug() << "Telescope coordinates - RA:" << centerRa << "° Dec:" << centerDec << "°";

    // Try surveys in order of preference
    QStringList surveyPriority = {"virgo_asteroids", "virgo_cluster", "rubin_dr0", "dss2_color"};
    
    for (const QString& surveyName : surveyPriority) {
        qDebug() << "Attempting to fetch tiles from survey:" << surveyName;
        
        if (attemptTileFetch(state, surveyName, order, nside, tileSize, width, height)) {
            qDebug() << "Successfully initiated fetch from" << surveyName;
            return;
        }
    }
    
    // If all surveys fail, generate synthetic image
    qWarning() << "All HiPS surveys failed, generating synthetic image";
    generateSyntheticImage(centerRa, centerDec);
}


QString RubinHipsClient::buildTileUrlForSurvey(const QString& surveyName, int order, int pix) {
    if (!m_surveys.contains(surveyName)) {
        return QString();
    }
    
    const auto& survey = m_surveys[surveyName];
    QString baseUrl = survey.base_url;
    
    // Different URL patterns for different services
    if (surveyName == "rubin_dr0") {
        int dir = (pix / 10000) * 10000;
        return QString("%1/Norder%2/Dir%3/Npix%4.webp")
               .arg(baseUrl).arg(order).arg(dir).arg(pix);
    }
    else if (surveyName.startsWith("virgo")) {
        int dir = (pix / 10000) * 10000;
        // FIXED: Rubin Observatory uses WebP format, not JPG
        return QString("%1/Norder%2/Dir%3/Npix%4.webp")
               .arg(baseUrl).arg(order).arg(dir).arg(pix);
    }
    else if (surveyName == "dss2_color" || surveyName == "2mass") {
        // Standard HiPS format for CDS services
        int dir = (pix / 10000) * 10000;
        return QString("%1/Norder%2/Dir%3/Npix%4.png")
               .arg(baseUrl).arg(order).arg(dir).arg(pix);
    }
    
    return QString();
}

bool RubinHipsClient::startTileDownload(const std::vector<TileJob>& jobs, 
                                        const QString& surveyName,
                                        int tileSize, int width, int height) {
    
    QImage* mosaic = new QImage(tileSize * width, tileSize * height, QImage::Format_RGB888);
    mosaic->fill(Qt::black);

    auto manager = new QNetworkAccessManager(this);
    int* completed = new int(0);
    int* successful = new int(0);
    int totalJobs = jobs.size();

    qDebug() << "Starting download of" << totalJobs << "tiles from" << surveyName;

    for (const auto& job : jobs) {
        QNetworkRequest request(QUrl(job.url));
        // Add proper headers matching the working browser request
        request.setHeader(QNetworkRequest::UserAgentHeader, 
            "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/18.5 Safari/605.1.15");
        request.setRawHeader("Accept", "image/webp,image/avif,image/jxl,image/heic,image/heic-sequence,video/*;q=0.8,image/png,image/svg+xml,image/*;q=0.8,*/*;q=0.5");
        request.setRawHeader("Accept-Encoding", "gzip, deflate, br");
        request.setRawHeader("Accept-Language", "en-GB,en;q=0.9");
        request.setRawHeader("Sec-Fetch-Dest", "image");
        request.setRawHeader("Sec-Fetch-Mode", "cors");
        request.setRawHeader("Sec-Fetch-Site", "cross-site");
        
        QNetworkReply *reply = manager->get(request);

        connect(reply, &QNetworkReply::finished, this, 
                [=, &job](){ handleTileReply(reply, job, mosaic, completed, successful, 
                                           totalJobs, surveyName, tileSize); });
    }

    return true;
}


void RubinHipsClient::addSurveyOverlay(QImage& image, const QString& surveyName, 
                                       int successful, int total) {
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Add survey information overlay
    painter.setPen(QColor(255, 255, 255, 200));
    painter.setFont(QFont("Arial", 12, QFont::Bold));
    
    QString surveyInfo = QString("%1\n%2/%3 tiles loaded")
                        .arg(m_surveys[surveyName].name)
                        .arg(successful)
                        .arg(total);
    
    QRect textRect = painter.fontMetrics().boundingRect(surveyInfo);
    QRect bgRect = textRect.adjusted(-10, -5, 10, 5);
    bgRect.moveTopLeft(QPoint(10, 10));
    
    // Semi-transparent background
    painter.fillRect(bgRect, QColor(0, 0, 0, 150));
    painter.drawText(bgRect.adjusted(10, 5, -10, -5), Qt::AlignLeft, surveyInfo);
    
    painter.end();
}
// Enhanced RubinHipsClient.cpp with detailed debugging and URL generation fixes

void RubinHipsClient::handleTileReply(QNetworkReply* reply, const TileJob& job, 
                                      QImage* mosaic, int* completed, int* successful,
                                      int totalJobs, const QString& surveyName, int tileSize) {
    
    (*completed)++;
    
    // ENHANCED DEBUGGING
    qDebug() << "=== Tile Reply Debug ===";
    qDebug() << "URL:" << reply->request().url().toString();
    qDebug() << "HTTP Status:" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    qDebug() << "Error:" << reply->error() << reply->errorString();
    qDebug() << "Content-Type:" << reply->header(QNetworkRequest::ContentTypeHeader).toString();
    
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        qDebug() << "Data size:" << data.size() << "bytes";
        qDebug() << "Data starts with:" << data.left(16).toHex();
        
        if (data.size() > 0) {
            QImage tile;
            
            // Try to load the image data
            if (tile.loadFromData(data)) {
                qDebug() << "✓ Image decoded successfully - size:" << tile.size();
                
                QPainter painter(mosaic);
                int x = job.tileX * tileSize;
                int y = job.tileY * tileSize;
                QImage scaledTile = tile.scaled(tileSize, tileSize, 
                                               Qt::KeepAspectRatio, 
                                               Qt::SmoothTransformation);
                painter.drawImage(x, y, scaledTile);
                painter.end();
                
                (*successful)++;
                qDebug() << "✓ Loaded tile" << job.pix << "from" << surveyName << "at position [" << job.tileX << "," << job.tileY << "]";
            } else {
                qDebug() << "✗ Failed to decode image data for tile" << job.pix;
                qDebug() << "  Data preview:" << data.left(64);
                
                // Try saving raw data to debug
                QString debugPath = m_imageDirectory + QString("/debug_tile_%1.webp").arg(job.pix);
                QFile debugFile(debugPath);
                if (debugFile.open(QIODevice::WriteOnly)) {
                    debugFile.write(data);
                    debugFile.close();
                    qDebug() << "  Saved raw data to:" << debugPath;
                }
            }
        } else {
            qDebug() << "✗ Empty response for tile" << job.pix;
        }
    } else {
        int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        qDebug() << "✗ Network error for tile" << job.pix << "- HTTP" << httpStatus << ":" << reply->errorString();
        
        // Read error response body
        QByteArray errorData = reply->readAll();
        if (!errorData.isEmpty()) {
            qDebug() << "  Error response:" << QString::fromUtf8(errorData.left(200));
        }
    }

    reply->deleteLater();

    qDebug() << "Progress:" << *completed << "/" << totalJobs 
             << "completed, " << *successful << "successful";

    // When all tiles are processed
    if (*completed == totalJobs) {
        QString result = QString("Completed %1 with %2/%3 successful tiles")
                        .arg(surveyName).arg(*successful).arg(totalJobs);
        qDebug() << result;
        
        // Save the mosaic regardless of success rate
        QString finalPath = m_imageDirectory + QString("/mosaic_%1.jpg").arg(surveyName);
        
        if (mosaic->save(finalPath, "JPG", 90)) {
            qDebug() << "✓ Saved mosaic to" << finalPath;
            
            // Add survey info overlay
            addSurveyOverlay(*mosaic, surveyName, *successful, totalJobs);
            mosaic->save(finalPath, "JPG", 90);
            
            if (onImageReady) onImageReady(finalPath);
        }
        
        // Clean up
        delete mosaic;
        delete completed;
        delete successful;
    }
}

// Also add debugging to see what URLs are actually being generated
bool RubinHipsClient::attemptTileFetch(TelescopeState *state, const QString& surveyName, 
                                       int order, int nside, int tileSize, int width, int height) {
    
    if (!m_surveys.contains(surveyName)) {
        return false;
    }
    
    double centerRa = state->ra * 180.0 / M_PI;
    double centerDec = state->dec * 180.0 / M_PI;

    try {
        Healpix_Base healpix(nside, NEST, SET_NSIDE);
        
        std::vector<TileJob> jobs;
        int validJobs = 0;

        for (int dy = 0; dy < height; ++dy) {
            for (int dx = 0; dx < width; ++dx) {
                // Calculate offset - smaller for higher order (order 10 = smaller tiles)
                double offsetRa = centerRa + (dx - width / 2.0 + 0.5) * 0.1;   // 0.1° per tile for order 10
                double offsetDec = centerDec - (dy - height / 2.0 + 0.5) * 0.1;

                // Normalize coordinates
                while (offsetRa < 0.0) offsetRa += 360.0;
                while (offsetRa >= 360.0) offsetRa -= 360.0;
                if (offsetDec < -90.0) offsetDec = -90.0;
                if (offsetDec > 90.0) offsetDec = 90.0;

                // Convert to HEALPix coordinates
                double theta = (90.0 - offsetDec) * M_PI / 180.0;
                double phi = offsetRa * M_PI / 180.0;

                validateAndClampCoordinates(theta, phi);

                pointing tilePoint(theta, phi);
                int pix = healpix.ang2pix(tilePoint);
                
                // Build URL for this survey
                QString url = buildTileUrlForSurvey(surveyName, order, pix);
                if (url.isEmpty()) continue;

                qDebug() << "Generated URL for tile [" << dx << "," << dy << "] pixel" << pix << ":" << url;
                jobs.push_back({pix, dx, dy, url});
                validJobs++;
            }
        }

        if (validJobs == 0) {
            qDebug() << "No valid jobs generated for survey" << surveyName;
            return false;
        }

        qDebug() << "Generated" << validJobs << "tile jobs for survey" << surveyName;
        
        // Start downloading tiles
        return startTileDownload(jobs, surveyName, tileSize, width, height);

    } catch (const std::exception& e) {
        qDebug() << "Error with survey" << surveyName << ":" << e.what();
        return false;
    }
}

/*
// Add a test method to verify a known working URL
void RubinHipsClient::testKnownWorkingURL() {
    // Test the URL you provided that works in the browser
    QString testUrl = "https://images.rubinobservatory.org/hips/asteroids/color_ugri/Norder10/Dir7090000/Npix7098582.webp";
    
    qDebug() << "Testing known working URL:" << testUrl;
    
    QNetworkRequest request(QUrl(testUrl));
    
    QNetworkReply *reply = m_networkManager->get(request);
    
    connect(reply, &QNetworkReply::finished, this, [=]() {
        qDebug() << "=== TEST URL RESPONSE ===";
        qDebug() << "HTTP Status:" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        qDebug() << "Error:" << reply->error() << reply->errorString();
        qDebug() << "Content-Type:" << reply->header(QNetworkRequest::ContentTypeHeader).toString();
        
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            qDebug() << "Response size:" << data.size() << "bytes";
            qDebug() << "Data starts with:" << data.left(16).toHex();
            
            QImage testImage;
            if (testImage.loadFromData(data)) {
                qDebug() << "✓ Test image loaded successfully! Size:" << testImage.size();
                
                // Save test image
                QString testPath = m_imageDirectory + "/test_working_tile.webp";
                QFile testFile(testPath);
                if (testFile.open(QIODevice::WriteOnly)) {
                    testFile.write(data);
                    testFile.close();
                    qDebug() << "✓ Saved test image to:" << testPath;
                }
            } else {
                qDebug() << "✗ Failed to decode test image";
            }
        }
        
        reply->deleteLater();
    });
}
*/

// Let's also add a method to compare our generated URLs with the working one
void RubinHipsClient::compareURLGeneration(TelescopeState *state) {
    double centerRa = state->ra * 180.0 / M_PI;
    double centerDec = state->dec * 180.0 / M_PI;
    
    qDebug() << "=== URL GENERATION COMPARISON ===";
    qDebug() << "Known working URL: https://images.rubinobservatory.org/hips/asteroids/color_ugri/Norder10/Dir7090000/Npix7098582.webp";
    qDebug() << "Known working pixel: 7098582";
    qDebug() << "Our center coordinates: RA" << centerRa << "° Dec" << centerDec << "°";
    
    try {
        int order = 10;
        int nside = 1 << order;
        Healpix_Base healpix(nside, NEST, SET_NSIDE);
        
        // Generate a few URLs around our center position
        for (int i = 0; i < 5; i++) {
            double testRa = centerRa + (i - 2) * 0.1;  // ±0.2° around center
            double testDec = centerDec;
            
            double theta = (90.0 - testDec) * M_PI / 180.0;
            double phi = testRa * M_PI / 180.0;
            
            pointing testPoint(theta, phi);
            int pix = healpix.ang2pix(testPoint);
            
            QString url = buildTileUrlForSurvey("virgo_asteroids", order, pix);
            qDebug() << "Generated URL" << i << "pixel" << pix << ":" << url;
        }
        
        // Try to reverse engineer the working pixel coordinates
        qDebug() << "=== REVERSE ENGINEERING WORKING PIXEL ===";
        int workingPixel = 7098582;
        pointing workingPoint = healpix.pix2ang(workingPixel);
        double workingTheta = workingPoint.theta;
        double workingPhi = workingPoint.phi;
        double workingRa = workingPhi * 180.0 / M_PI;
        double workingDec = 90.0 - workingTheta * 180.0 / M_PI;
        
        qDebug() << "Working pixel" << workingPixel << "corresponds to:";
        qDebug() << "  RA:" << workingRa << "° Dec:" << workingDec << "°";
        qDebug() << "  Distance from our center:" << 
                    sqrt(pow(workingRa - centerRa, 2) + pow(workingDec - centerDec, 2)) << "°";
        
    } catch (const std::exception& e) {
        qDebug() << "Error in URL comparison:" << e.what();
    }
}
