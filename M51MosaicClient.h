// M51MosaicClient.h - Enhanced HiPS client for creating M51 RGB mosaic
#ifndef M51MOSAICCLIENT_H
#define M51MOSAICCLIENT_H

#include "ProperHipsClient.h"
#include <QPixmap>
#include <QImage>
#include <QPainter>
#include <QProgressBar>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

struct MosaicTile {
    int gridX, gridY;           // Position in tile grid
    QString survey;             // Survey name (DSS2_Color, 2MASS_Color, etc.)
    QImage image;              // Downloaded tile image
    SkyPosition skyPosition;    // Sky coordinates of tile center
    long long healpixPixel;    // HEALPix pixel number
    int order;                 // HiPS order
    bool downloaded;           // Download status
    QString url;               // Tile URL
};

struct MosaicConfig {
    // Target specifications
    int outputWidth = 800;      // Final mosaic width in pixels
    int outputHeight = 600;     // Final mosaic height in pixels
    double targetResolution = 1.0; // Target arcsec/pixel
    
    // M51 specifications
    double centerRA = 202.4695833;   // 13h 29m 52.7s
    double centerDec = 47.1951667;   // +47° 11' 42.6"
    double fieldWidthArcsec = 800;   // 13.33 arcmin
    double fieldHeightArcsec = 600;  // 10 arcmin
    
    // HiPS parameters
    int hipsOrder = 10;         // Start with order 10, fallback to 8,6
    QStringList surveyPriority = {"DSS2_Color", "2MASS_Color", "2MASS_J"};
};

class M51MosaicClient : public QWidget {
    Q_OBJECT

public:
    explicit M51MosaicClient(QWidget *parent = nullptr);
    
    // Main interface
    void createMosaic();
    void setConfig(const MosaicConfig& config) { m_config = config; }
    
    // Progress monitoring
    int getTotalTiles() const { return m_tiles.size(); }
    int getCompletedTiles() const;
    double getProgress() const;
    
    // Results
    QImage getFinalMosaic() const { return m_finalMosaic; }
    void saveMosaic(const QString& filename) const;
    
signals:
    void mosaicProgress(int completed, int total);
    void mosaicComplete(const QImage& mosaic);
    void tileDownloaded(int x, int y, const QString& survey);
    void errorOccurred(const QString& error);

private slots:
    void onTileDownloaded();
    void onAllTilesComplete();

private:
    ProperHipsClient* m_hipsClient;
    MosaicConfig m_config;
    QList<MosaicTile> m_tiles;
    QImage m_finalMosaic;
    
    // UI components
    QProgressBar* m_progressBar;
    QLabel* m_statusLabel;
    QLabel* m_previewLabel;
    
    // Internal methods
    void calculateTileGrid();
    void startDownloads();
    void downloadTile(MosaicTile& tile);
    void assembleMosaic();
    void updateProgress();
    void testMultipleOrders();
    
    // Coordinate transformations
    SkyPosition calculateTileCenter(int gridX, int gridY) const;
    QRect calculateTileRect(int gridX, int gridY) const;
    
    // Image processing
    QImage scaleTileToTarget(const QImage& sourceImage, double sourceResolution) const;
    QImage blendTiles(const QList<QImage>& tiles, const QList<QRect>& positions) const;
    
    // Fallback strategies
    void tryLowerOrder();
    void createScaledMosaic();
};

// Implementation of key methods
M51MosaicClient::M51MosaicClient(QWidget *parent) : QWidget(parent) {
    m_hipsClient = new ProperHipsClient(this);
    
    // Setup UI
    setWindowTitle("M51 Whirlpool Galaxy Mosaic Creator");
    setMinimumSize(600, 400);
    
    QVBoxLayout* layout = new QVBoxLayout(this);
    
    m_statusLabel = new QLabel("Ready to create M51 mosaic", this);
    m_progressBar = new QProgressBar(this);
    m_previewLabel = new QLabel(this);
    m_previewLabel->setMinimumSize(400, 300);
    m_previewLabel->setScaledContents(true);
    m_previewLabel->setStyleSheet("border: 1px solid gray;");
    
    layout->addWidget(m_statusLabel);
    layout->addWidget(m_progressBar);
    layout->addWidget(m_previewLabel);
    
}

void M51MosaicClient::createMosaic() {
    m_statusLabel->setText("Calculating tile grid for M51...");
    
    // Test what orders actually work for M51 region
    testMultipleOrders();
    
    calculateTileGrid();
    startDownloads();
}

void M51MosaicClient::testMultipleOrders() {
    // Test M51 position with different orders to see what works
    SkyPosition m51Center = {m_config.centerRA, m_config.centerDec, "M51_Center", "Test position"};
    
    // Try orders from high to low resolution
    QList<int> testOrders = {12, 11, 10, 9, 8, 7, 6};
    
    m_statusLabel->setText("Testing HiPS orders for M51 region...");
    
    for (int order : testOrders) {
        for (const QString& survey : m_config.surveyPriority) {
            QString testUrl = m_hipsClient->buildTileUrl(survey, m51Center, order);
            
            // Quick HEAD request to test if URL exists
            // Implementation would go here - for now assume Order 6-8 work
            if (order <= 8) {
                m_config.hipsOrder = order;
                qDebug() << "Using HiPS Order" << order << "for" << survey;
                return;
            }
        }
    }
    
    // Fallback to minimum working order
    m_config.hipsOrder = 6;
    qDebug() << "Falling back to Order 6";
}

void M51MosaicClient::calculateTileGrid() {
    m_tiles.clear();
    
    // Calculate how many tiles we need based on the HiPS order
    double arcsecPerPixel;
    switch(m_config.hipsOrder) {
        case 6:  arcsecPerPixel = 450.0; break;     // 7.5 arcmin/pixel
        case 7:  arcsecPerPixel = 225.0; break;     // 3.75 arcmin/pixel  
        case 8:  arcsecPerPixel = 112.5; break;     // 1.875 arcmin/pixel
        case 9:  arcsecPerPixel = 56.25; break;     // 56.25 arcsec/pixel
        case 10: arcsecPerPixel = 28.125; break;    // 28.125 arcsec/pixel
        default: arcsecPerPixel = 450.0; break;     // Default to order 6
    }
    arcsecPerPixel /= 60.0;
    
    double arcsecPerTile = arcsecPerPixel * 512; // Each tile is 512x512 pixels
    
    // Calculate tile grid size
    int tilesWide = std::max(1, (int)ceil(m_config.fieldWidthArcsec / arcsecPerTile));
    int tilesHigh = std::max(1, (int)ceil(m_config.fieldHeightArcsec / arcsecPerTile));
    
    qDebug() << QString("Tile grid: %1x%2 tiles, %3 arcsec/pixel, %4 arcsec/tile")
                .arg(tilesWide).arg(tilesHigh).arg(arcsecPerPixel).arg(arcsecPerTile);
    
    // Create tile objects
    for (int y = 0; y < tilesHigh; y++) {
        for (int x = 0; x < tilesWide; x++) {
            MosaicTile tile;
            tile.gridX = x;
            tile.gridY = y;
            tile.skyPosition = calculateTileCenter(x, y);
            tile.order = m_config.hipsOrder;
            tile.downloaded = false;
            tile.survey = m_config.surveyPriority.first(); // Start with best survey
            
            m_tiles.append(tile);
        }
    }
    
    m_statusLabel->setText(QString("Calculated %1 tiles to download").arg(m_tiles.size()));
}

SkyPosition M51MosaicClient::calculateTileCenter(int gridX, int gridY) const {
    // Calculate sky position for tile center
    double arcsecPerPixel = 450.0 / pow(2, m_config.hipsOrder - 6);
    double arcsecPerTile = arcsecPerPixel * 512;
    
    // Offset from center position
    double offsetRA = (gridX - 0.5) * arcsecPerTile / cos(m_config.centerDec * M_PI / 180.0);
    double offsetDec = (gridY - 0.5) * arcsecPerTile;
    
    SkyPosition pos;
    pos.ra_deg = m_config.centerRA + offsetRA / 3600.0;  // Convert arcsec to degrees
    pos.dec_deg = m_config.centerDec + offsetDec / 3600.0;
    pos.name = QString("M51_Tile_%1_%2").arg(gridX).arg(gridY);
    pos.description = QString("Tile at grid position %1,%2").arg(gridX).arg(gridY);
    
    return pos;
}

void M51MosaicClient::startDownloads() {
    m_statusLabel->setText("Starting tile downloads...");
    m_progressBar->setRange(0, m_tiles.size());
    m_progressBar->setValue(0);
    
    // Start downloading tiles (implement sequential or parallel downloads)
    for (MosaicTile& tile : m_tiles) {
        downloadTile(tile);
    }
}

void M51MosaicClient::downloadTile(MosaicTile& tile) {
    // Build URL for this tile
    tile.url = m_hipsClient->buildTileUrl(tile.survey, tile.skyPosition, tile.order);
    
    if (tile.url.isEmpty()) {
        qDebug() << "Failed to build URL for tile" << tile.gridX << tile.gridY;
        
        // Try next survey in priority list
        int currentIndex = m_config.surveyPriority.indexOf(tile.survey);
        if (currentIndex < m_config.surveyPriority.size() - 1) {
            tile.survey = m_config.surveyPriority[currentIndex + 1];
            downloadTile(tile); // Recursive retry with different survey
            return;
        }
        
        // Mark as failed if no surveys work
        tile.downloaded = false;
        emit errorOccurred(QString("No working surveys for tile %1,%2").arg(tile.gridX).arg(tile.gridY));
        return;
    }
    
    // Calculate HEALPix pixel for this position
    tile.healpixPixel = m_hipsClient->calculateHealPixel(tile.skyPosition, tile.order);
    
    qDebug() << QString("Downloading tile %1,%2 from %3").arg(tile.gridX).arg(tile.gridY).arg(tile.survey);
    qDebug() << "URL:" << tile.url;
    
    // Use the existing HiPS client to download
    m_hipsClient->testSurveyAtPosition(tile.survey, tile.skyPosition);
}

void M51MosaicClient::onTileDownloaded() {
    // This would be called when HiPS client completes a download
    updateProgress();
    
    // Check if all tiles are complete
    if (getCompletedTiles() >= getTotalTiles()) {
        onAllTilesComplete();
    }
}

void M51MosaicClient::onAllTilesComplete() {
    m_statusLabel->setText("All tiles downloaded. Assembling mosaic...");
    assembleMosaic();
}

void M51MosaicClient::assembleMosaic() {
    // Create final mosaic image
    m_finalMosaic = QImage(m_config.outputWidth, m_config.outputHeight, QImage::Format_RGB32);
    m_finalMosaic.fill(Qt::black);
    
    QPainter painter(&m_finalMosaic);
    
    // Calculate scaling factors
    double arcsecPerPixel = 450.0 / pow(2, m_config.hipsOrder - 6);
    double scaleFactor = arcsecPerPixel / m_config.targetResolution;
    
    // Place each tile
    for (const MosaicTile& tile : m_tiles) {
        if (!tile.downloaded || tile.image.isNull()) continue;
        
        // Calculate position in final mosaic
        QRect targetRect = calculateTileRect(tile.gridX, tile.gridY);
        
        // Scale tile image to target resolution
        QImage scaledTile = scaleTileToTarget(tile.image, arcsecPerPixel);
        
        // Draw onto final mosaic
        painter.drawImage(targetRect, scaledTile);
        
        qDebug() << QString("Placed tile %1,%2 at %3,%4 size %5x%6")
                    .arg(tile.gridX).arg(tile.gridY)
                    .arg(targetRect.x()).arg(targetRect.y())
                    .arg(targetRect.width()).arg(targetRect.height());
    }
    
    painter.end();
    
    // Update preview
    QPixmap preview = QPixmap::fromImage(m_finalMosaic.scaled(400, 300, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_previewLabel->setPixmap(preview);
    
    m_statusLabel->setText(QString("Mosaic complete! %1x%2 pixels covering M51").arg(m_config.outputWidth).arg(m_config.outputHeight));
    
    emit mosaicComplete(m_finalMosaic);
}

QRect M51MosaicClient::calculateTileRect(int gridX, int gridY) const {
    // Calculate where this tile should be placed in the final mosaic
    double arcsecPerPixel = 450.0 / pow(2, m_config.hipsOrder - 6);
    double arcsecPerTile = arcsecPerPixel * 512;
    
    // Convert to pixels in final mosaic
    int pixelsPerTile = (int)(arcsecPerTile / m_config.targetResolution);
    
    int x = gridX * pixelsPerTile;
    int y = gridY * pixelsPerTile;
    
    // Ensure we don't exceed mosaic bounds
    int width = std::min(pixelsPerTile, m_config.outputWidth - x);
    int height = std::min(pixelsPerTile, m_config.outputHeight - y);
    
    return QRect(x, y, width, height);
}

QImage M51MosaicClient::scaleTileToTarget(const QImage& sourceImage, double sourceResolution) const {
    // Scale the source image to match target resolution
    double scaleFactor = sourceResolution / m_config.targetResolution;
    
    if (scaleFactor > 1.0) {
        // Source is lower resolution - scale up
        int newWidth = (int)(sourceImage.width() * scaleFactor);
        int newHeight = (int)(sourceImage.height() * scaleFactor);
        return sourceImage.scaled(newWidth, newHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    } else {
        // Source is higher resolution - scale down
        int newWidth = (int)(sourceImage.width() / scaleFactor);
        int newHeight = (int)(sourceImage.height() / scaleFactor);
        return sourceImage.scaled(newWidth, newHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
}

int M51MosaicClient::getCompletedTiles() const {
    int completed = 0;
    for (const MosaicTile& tile : m_tiles) {
        if (tile.downloaded) completed++;
    }
    return completed;
}

double M51MosaicClient::getProgress() const {
    if (m_tiles.isEmpty()) return 0.0;
    return (double)getCompletedTiles() / (double)getTotalTiles();
}

void M51MosaicClient::updateProgress() {
    int completed = getCompletedTiles();
    m_progressBar->setValue(completed);
    
    double percentage = getProgress() * 100.0;
    m_statusLabel->setText(QString("Downloaded %1/%2 tiles (%3%)")
                          .arg(completed).arg(getTotalTiles()).arg(percentage, 0, 'f', 1));
    
    emit mosaicProgress(completed, getTotalTiles());
}

void M51MosaicClient::saveMosaic(const QString& filename) const {
    if (m_finalMosaic.isNull()) {
        qDebug() << "No mosaic to save";
        return;
    }
    
    bool success = m_finalMosaic.save(filename);
    qDebug() << "Mosaic saved to" << filename << ":" << (success ? "SUCCESS" : "FAILED");
}

void M51MosaicClient::tryLowerOrder() {
    // Fallback strategy if current order fails
    if (m_config.hipsOrder > 6) {
        m_config.hipsOrder--;
        qDebug() << "Falling back to HiPS Order" << m_config.hipsOrder;
        calculateTileGrid();
        startDownloads();
    } else {
        emit errorOccurred("All HiPS orders failed - cannot create mosaic");
    }
}

#endif // M51MOSAICCLIENT_H

// Usage example:
/*
int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    M51MosaicClient mosaicClient;
    
    // Configure for M51
    MosaicConfig config;
    config.outputWidth = 800;
    config.outputHeight = 600;
    config.targetResolution = 4.0;  // 4 arcsec/pixel (more realistic)
    config.hipsOrder = 8;           // Start with order 8
    
    mosaicClient.setConfig(config);
    mosaicClient.show();
    
    // Start mosaic creation
    mosaicClient.createMosaic();
    
    return app.exec();
}
*/
