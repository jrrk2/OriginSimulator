// main_m51_mosaic_simple.cpp - Simple 3x3 grid version
#include <QApplication>
#include <QDebug>
#include <QTimer>
#include <QDir>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>
#include <QNetworkRequest>
#include <QPixmap>
#include <QImage>
#include <QPainter>
#include <QFile>
#include "ProperHipsClient.h"

class M51MosaicCreator : public QObject {
    Q_OBJECT

public:
    explicit M51MosaicCreator(QObject *parent = nullptr);
    void createSimpleMosaic(SkyPosition);

private slots:
    void onTileDownloaded();
    void processNextTile();
    void assembleFinalMosaic();

private:
    ProperHipsClient* m_hipsClient;
    QNetworkAccessManager* m_networkManager;
  
    // Simple tile structure
    struct SimpleTile {
        int gridX, gridY;
        long long healpixPixel;
        QString filename;
        QString url;
        QImage image;
        bool downloaded;
    };
    
    QList<SimpleTile> m_tiles;
    int m_currentTileIndex;
    QString m_outputDir;
    QDateTime m_downloadStartTime;
    
    void createTileGrid(SkyPosition);
    void downloadTile(int tileIndex);
    void saveProgressReport();
};

M51MosaicCreator::M51MosaicCreator(QObject *parent) : QObject(parent) {
    m_hipsClient = new ProperHipsClient(this);
    m_networkManager = new QNetworkAccessManager(this);
    m_currentTileIndex = 0;
    
    // Create output directory
    m_outputDir = "m51_mosaic_tiles";
    QDir().mkpath(m_outputDir);
    
    qDebug() << "=== M51 Simple Mosaic Creator ===";
    qDebug() << "Just placing tiles in a 3x3 grid - no fancy coordinate stuff!";
}

void M51MosaicCreator::createSimpleMosaic(SkyPosition pos) {
    qDebug() << "\n=== Creating Simple Mosaic ===";
    
    createTileGrid(pos);
    
    qDebug() << QString("\nStarting download of %1 tiles...").arg(m_tiles.size());
    m_currentTileIndex = 0;
    processNextTile();
}

void M51MosaicCreator::createTileGrid(SkyPosition pos) {
    m_tiles.clear();
    int order = 8;
    // Test M51 position with working survey
    long long m51Pixel = m_hipsClient->calculateHealPixel(pos, order);
    QList<QList<long long>> pgrid = m_hipsClient->createProper3x3Grid(m51Pixel, order);
    
    qDebug() << "Creating 3×3 tile grid:";
    
    for (int y = 0; y < 3; y++) {
        for (int x = 0; x < 3; x++) {
            SimpleTile tile;
            tile.gridX = x;
            tile.gridY = y;
            tile.healpixPixel = pgrid[y][x];
	    qDebug() << "pgrid: " << pgrid[y][x];
            tile.downloaded = false;
            
            // Build filename and URL directly from HEALPix pixel
            tile.filename = QString("%1/simple_tile_%2_%3_pixel%4.jpg")
                           .arg(m_outputDir).arg(x).arg(y).arg(tile.healpixPixel);
            
            int dir = (tile.healpixPixel / 10000) * 10000;
            tile.url = QString("http://alasky.u-strasbg.fr/DSS/DSSColor/Norder%1/Dir%2/Npix%3.jpg")
                      .arg(order).arg(dir).arg(tile.healpixPixel);
            
            if (tile.healpixPixel == 176440) {
                qDebug() << QString("  Grid(%1,%2): HEALPix %3 ★ M51 TILE! ★")
                            .arg(x).arg(y).arg(tile.healpixPixel);
            } else {
                qDebug() << QString("  Grid(%1,%2): HEALPix %3")
                            .arg(x).arg(y).arg(tile.healpixPixel);
            }
            
            m_tiles.append(tile);
        }
    }
    
    qDebug() << QString("Created simple %1 tile grid").arg(m_tiles.size());
}

void M51MosaicCreator::processNextTile() {
    if (m_currentTileIndex >= m_tiles.size()) {
        assembleFinalMosaic();
        return;
    }
    
    downloadTile(m_currentTileIndex);
}

void M51MosaicCreator::downloadTile(int tileIndex) {
    if (tileIndex >= m_tiles.size()) return;
    
    const SimpleTile& tile = m_tiles[tileIndex];
    
    qDebug() << QString("Downloading tile %1/%2: Grid(%3,%4) HEALPix %5")
                .arg(tileIndex + 1).arg(m_tiles.size())
                .arg(tile.gridX).arg(tile.gridY)
                .arg(tile.healpixPixel);
    qDebug() << QString("URL: %1").arg(tile.url);
    
    // Create network request
    QNetworkRequest request(QUrl(tile.url));
    request.setHeader(QNetworkRequest::UserAgentHeader, "M51SimpleMosaicCreator/1.0");
    request.setRawHeader("Accept", "image/*");
    
    m_downloadStartTime = QDateTime::currentDateTime();
    QNetworkReply* reply = m_networkManager->get(request);
    
    // Store tile index in reply
    reply->setProperty("tileIndex", tileIndex);
    
    connect(reply, &QNetworkReply::finished, this, &M51MosaicCreator::onTileDownloaded);
    
    // Set timeout
    QTimer::singleShot(15000, reply, &QNetworkReply::abort);
}

void M51MosaicCreator::onTileDownloaded() {
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    int tileIndex = reply->property("tileIndex").toInt();
    if (tileIndex >= m_tiles.size()) {
        reply->deleteLater();
        return;
    }
    
    SimpleTile& tile = m_tiles[tileIndex];
    
    if (reply->error() == QNetworkReply::NoError) {
        // Read image data
        QByteArray imageData = reply->readAll();
        
        // Load image
        tile.image.loadFromData(imageData);
        
        if (!tile.image.isNull()) {
            // Save image to file
            bool saved = tile.image.save(tile.filename);
            tile.downloaded = true;
            
            qint64 downloadTime = m_downloadStartTime.msecsTo(QDateTime::currentDateTime());
            
            qDebug() << QString("✅ Tile %1/%2 downloaded: %3ms, %4 bytes, %5x%6 pixels%7")
                        .arg(tileIndex + 1).arg(m_tiles.size())
                        .arg(downloadTime)
                        .arg(imageData.size())
                        .arg(tile.image.width()).arg(tile.image.height())
                        .arg(saved ? ", saved" : ", save failed");
        } else {
            qDebug() << QString("❌ Tile %1/%2 - invalid image data")
                        .arg(tileIndex + 1).arg(m_tiles.size());
        }
    } else {
        qDebug() << QString("❌ Tile %1/%2 download failed: %3")
                    .arg(tileIndex + 1).arg(m_tiles.size())
                    .arg(reply->errorString());
    }
    
    reply->deleteLater();
    
    m_currentTileIndex++;
    
    // Small delay between downloads
    QTimer::singleShot(500, this, &M51MosaicCreator::processNextTile);
}

void M51MosaicCreator::assembleFinalMosaic() {
    qDebug() << "\n=== Assembling Simple M51 Mosaic ===";
    
    int successfulTiles = 0;
    for (const SimpleTile& tile : m_tiles) {
        if (tile.downloaded && !tile.image.isNull()) {
            successfulTiles++;
        }
    }
    
    qDebug() << QString("Downloaded %1/%2 tiles").arg(successfulTiles).arg(m_tiles.size());
    
    if (successfulTiles == 0) {
        qDebug() << "❌ No tiles downloaded successfully";
        QTimer::singleShot(1000, qApp, &QApplication::quit);
        return;
    }
    
    // Create simple 3x3 mosaic: 3*512 = 1536 pixels
    int tileSize = 512;
    int mosaicSize = 3 * tileSize; // 1536x1536
    
    QImage finalMosaic(mosaicSize, mosaicSize, QImage::Format_RGB32);
    finalMosaic.fill(Qt::black);
    
    QPainter painter(&finalMosaic);
    
    int tilesPlaced = 0;
    
    qDebug() << "Placing tiles in simple grid:";
    
    for (const SimpleTile& tile : m_tiles) {
        if (!tile.downloaded || tile.image.isNull()) {
            qDebug() << QString("  Skipping tile %1,%2 - not downloaded").arg(tile.gridX).arg(tile.gridY);
            continue;
        }
        
        // Simple placement: gridX * 512, gridY * 512
        int pixelX = tile.gridX * tileSize;
        int pixelY = tile.gridY * tileSize;
        
        // Draw the tile
        painter.drawImage(pixelX, pixelY, tile.image);
        
        tilesPlaced++;
        
        if (tile.healpixPixel == 176440) {
            qDebug() << QString("  ✅ PLACED M51 TILE (%1,%2) at pixel (%3,%4)")
                        .arg(tile.gridX).arg(tile.gridY).arg(pixelX).arg(pixelY);
        } else {
            qDebug() << QString("  ✅ Placed tile (%1,%2) at pixel (%3,%4)")
                        .arg(tile.gridX).arg(tile.gridY).arg(pixelX).arg(pixelY);
        }
    }
    
    // Add simple crosshairs at M51 location (center of middle tile)
    painter.setPen(QPen(Qt::yellow, 3));
    int m51X = 1 * tileSize + tileSize/2; // Center of grid position (1,1)
    int m51Y = 1 * tileSize + tileSize/2;
    
    // Draw crosshairs
    painter.drawLine(m51X - 30, m51Y, m51X + 30, m51Y);
    painter.drawLine(m51X, m51Y - 30, m51X, m51Y + 30);
    
    // Add label
    painter.setPen(QPen(Qt::yellow, 1));
    painter.setFont(QFont("Arial", 14, QFont::Bold));
    painter.drawText(m51X + 40, m51Y - 10, "M51");
    
    painter.end();
    
    // Save final mosaic
    QString mosaicFilename = QString("%1/m51_simple_mosaic_3x3.png").arg(m_outputDir);
    bool saved = finalMosaic.save(mosaicFilename);
    
    qDebug() << QString("\n🖼️  Simple mosaic complete!");
    qDebug() << QString("📁 Size: %1×%2 pixels (%3 tiles placed)")
                .arg(mosaicSize).arg(mosaicSize).arg(tilesPlaced);
    qDebug() << QString("📁 Saved to: %1 (%2)")
                .arg(mosaicFilename).arg(saved ? "SUCCESS" : "FAILED");
    
    // Also save a smaller preview
    QString previewFilename = QString("%1/m51_simple_preview.jpg").arg(m_outputDir);
    QImage preview = finalMosaic.scaled(512, 512, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    preview.save(previewFilename);
    qDebug() << QString("📁 Preview: %1").arg(previewFilename);
    
    saveProgressReport();
    
    qDebug() << "\n🎯 SIMPLE M51 MOSAIC COMPLETE!";
    qDebug() << "✅ M51 should be clearly visible in the center tile with crosshairs";
    
    QTimer::singleShot(2000, qApp, &QApplication::quit);
}

void M51MosaicCreator::saveProgressReport() {
    QString reportFile = QString("%1/simple_mosaic_report.txt").arg(m_outputDir);
    QFile file(reportFile);
    
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "Could not save progress report";
        return;
    }
    
    QTextStream out(&file);
    out << "M51 Simple Mosaic Report\n";
    out << "Generated: " << QDateTime::currentDateTime().toString() << "\n\n";
    
    out << "Simple 3x3 Grid Layout:\n";
    out << "Grid_X,Grid_Y,HEALPix_Pixel,Downloaded,ImageSize,Filename\n";
    
    for (const SimpleTile& tile : m_tiles) {
        out << QString("%1,%2,%3,%4,%5x%6,%7\n")
               .arg(tile.gridX).arg(tile.gridY)
               .arg(tile.healpixPixel)
               .arg(tile.downloaded ? "YES" : "NO")
               .arg(tile.image.width()).arg(tile.image.height())
               .arg(tile.filename);
    }
    
    file.close();
    qDebug() << "Report saved:" << reportFile;
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    SkyPosition argpos = {argc > 4 ? atof(argv[1]) : 0.0, argc > 4 ? atof(argv[2]) : 0.0, argc > 4 ? argv[3] : "", argc > 4 ? argv[4] : ""};
    SkyPosition dfltpos = {202.4695833, 47.1951667, "M51", "Whirlpool Galaxy"};
    SkyPosition pos = argc > 4 ? argpos : dfltpos;

    qDebug() << "Simple Mosaic Creator";
    qDebug() << "No fancy coordinates - just a simple 3x3 grid!\n";
    
    M51MosaicCreator creator;
    creator.createSimpleMosaic(pos);
    
    return app.exec();
}

#include "main_m51_mosaic.moc"
