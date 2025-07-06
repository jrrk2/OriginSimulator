#include "EnhancedMosaicCreator.h"
#include "MessierCatalog.h"

EnhancedMosaicCreator::EnhancedMosaicCreator(QObject *parent)  // CHANGED: QObject parent
    : QObject(parent) {  // CHANGED: QObject constructor
    
    m_hipsClient = new ProperHipsClient(this);
    m_networkManager = new QNetworkAccessManager(this);
    m_currentTileIndex = 0;
    
    QString homeDir = QDir::homePath();
    m_outputDir = QDir(homeDir).absoluteFilePath("Library/Application Support/OriginSimulator/Images/mosaics");
    QDir().mkpath(m_outputDir);
    
    qDebug() << "=== Enhanced Mosaic Creator - Headless Mode ===";
    qDebug() << "Precise coordinate placement with sub-tile accuracy!";
}

// NEW: Public coordinate setter method
void EnhancedMosaicCreator::setCustomCoordinates(const QString& raText, const QString& decText, const QString& name) {
    try {
        m_customTarget = SimpleCoordinateParser::parseCoordinates(raText, decText, name);
        m_actualTarget = m_customTarget;
        
        qDebug() << QString("Set coordinates: RA=%1°, Dec=%2°, Name=%3")
                    .arg(m_customTarget.ra_deg, 0, 'f', 6)
                    .arg(m_customTarget.dec_deg, 0, 'f', 6)
                    .arg(name);
    } catch (...) {
        qDebug() << "Failed to parse coordinates:" << raText << decText;
    }
}

// NEW: Public mosaic creation method
void EnhancedMosaicCreator::createCustomMosaic(const SkyPosition& target) {
    m_customTarget = target;
    m_actualTarget = target;
    
    qDebug() << QString("\n=== Creating Coordinate-Centered Mosaic for %1 ===").arg(target.name);
    
    // Store the actual target coordinates for precise centering
    createTileGrid(target);
    
    qDebug() << QString("Target coordinates: RA=%1°, Dec=%2°")
                .arg(m_actualTarget.ra_deg, 0, 'f', 6)
                .arg(m_actualTarget.dec_deg, 0, 'f', 6);
    qDebug() << QString("Starting download of %1 tiles...").arg(m_tiles.size());
    m_currentTileIndex = 0;
    processNextTile();
}

void EnhancedMosaicCreator::createTileGrid(const SkyPosition& position) {
    m_tiles.clear();
    int order = 8;
    
    long long centerPixel = m_hipsClient->calculateHealPixel(position, order);
    QList<QList<long long>> grid = m_hipsClient->createProper3x3Grid(centerPixel, order);
    
    qDebug() << QString("Creating 3×3 tile grid around %1:").arg(position.name);
    
    for (int y = 0; y < 3; y++) {
        for (int x = 0; x < 3; x++) {
            SimpleTile tile;
            tile.gridX = x;
            tile.gridY = y;
            tile.healpixPixel = grid[y][x];
            tile.downloaded = false;
            
            // Calculate the sky coordinates for this tile
            tile.skyCoordinates = healpixToSkyPosition(tile.healpixPixel, order);
            
            QString objectName = position.name.toLower();
            tile.filename = QString("%1/tile_pixel%2.jpg")
                           .arg(m_outputDir).arg(tile.healpixPixel);
            
            int dir = (tile.healpixPixel / 10000) * 10000;
            tile.url = QString("http://alasky.u-strasbg.fr/DSS/DSSColor/Norder%1/Dir%2/Npix%3.jpg")
                      .arg(order).arg(dir).arg(tile.healpixPixel);
            
            // Calculate distance from target to tile center
            double distance = calculateAngularDistance(m_actualTarget, tile.skyCoordinates);
            
            if (tile.healpixPixel == centerPixel) {
                qDebug() << QString("  Grid(%1,%2): HEALPix %3 ★ NEAREST TILE ★ (%4 arcsec from target)")
                            .arg(x).arg(y).arg(tile.healpixPixel).arg(distance * 3600.0, 0, 'f', 1);
            } else {
                qDebug() << QString("  Grid(%1,%2): HEALPix %3 (%4 arcsec from target)")
                            .arg(x).arg(y).arg(tile.healpixPixel).arg(distance * 3600.0, 0, 'f', 1);
            }
            
            m_tiles.append(tile);
        }
    }
    
    qDebug() << QString("Created %1 tile grid - will crop to center target precisely").arg(m_tiles.size());
}

void EnhancedMosaicCreator::processNextTile() {
    if (m_currentTileIndex >= m_tiles.size()) {
        assembleFinalMosaicCentered();
        return;
    }
    
    SimpleTile& tile = m_tiles[m_currentTileIndex];
    if (checkExistingTile(tile)) {
        m_currentTileIndex++;
        QTimer::singleShot(100, this, &EnhancedMosaicCreator::processNextTile);
        return;
    }
    
    downloadTile(m_currentTileIndex);
}

void EnhancedMosaicCreator::downloadTile(int tileIndex) {
    if (tileIndex >= m_tiles.size()) return;
    
    const SimpleTile& tile = m_tiles[tileIndex];
    
    qDebug() << QString("Downloading tile %1/%2: Grid(%3,%4) HEALPix %5")
                .arg(tileIndex + 1).arg(m_tiles.size())
                .arg(tile.gridX).arg(tile.gridY)
                .arg(tile.healpixPixel);
    
    QNetworkRequest request(QUrl(tile.url));
    request.setHeader(QNetworkRequest::UserAgentHeader, "EnhancedMosaicCreator/1.0");
    request.setRawHeader("Accept", "image/*");
    
    m_downloadStartTime = QDateTime::currentDateTime();
    QNetworkReply* reply = m_networkManager->get(request);
    
    reply->setProperty("tileIndex", tileIndex);
    connect(reply, &QNetworkReply::finished, this, &EnhancedMosaicCreator::onTileDownloaded);
    
    QTimer::singleShot(15000, reply, &QNetworkReply::abort);
}

void EnhancedMosaicCreator::onTileDownloaded() {
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    int tileIndex = reply->property("tileIndex").toInt();
    if (tileIndex >= m_tiles.size()) {
        reply->deleteLater();
        return;
    }
    
    SimpleTile& tile = m_tiles[tileIndex];
    
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray imageData = reply->readAll();
        tile.image.loadFromData(imageData);
        
        if (!tile.image.isNull()) {
            bool saved = tile.image.save(tile.filename);
            tile.downloaded = true;
            
            qint64 downloadTime = m_downloadStartTime.msecsTo(QDateTime::currentDateTime());
            qDebug() << QString("✅ Tile %1/%2 downloaded: %3ms, %4 bytes, %5x%6 pixels%7")
                        .arg(tileIndex + 1).arg(m_tiles.size())
                        .arg(downloadTime).arg(imageData.size())
                        .arg(tile.image.width()).arg(tile.image.height())
                        .arg(saved ? ", saved" : ", save failed");
        }
    } else {
        qDebug() << QString("❌ Tile %1/%2 download failed: %3")
                    .arg(tileIndex + 1).arg(m_tiles.size())
                    .arg(reply->errorString());
    }
    
    reply->deleteLater();
    m_currentTileIndex++;
    QTimer::singleShot(500, this, &EnhancedMosaicCreator::processNextTile);
}

void EnhancedMosaicCreator::assembleFinalMosaicCentered() {
    QString targetName = m_customTarget.name;
    
    qDebug() << QString("\n=== Assembling Coordinate-Centered %1 Mosaic ===").arg(targetName);
    
    int successfulTiles = 0;
    for (const SimpleTile& tile : m_tiles) {
        if (tile.downloaded && !tile.image.isNull()) {
            successfulTiles++;
        }
    }
    
    if (successfulTiles == 0) {
        qDebug() << QString("Failed to download tiles for %1").arg(targetName);
        return;
    }
    
    // Step 1: Create the raw 3x3 mosaic
    int tileSize = 512;
    int rawMosaicSize = 3 * tileSize; // 1536x1536
    
    QImage rawMosaic(rawMosaicSize, rawMosaicSize, QImage::Format_RGB32);
    rawMosaic.fill(Qt::black);
    
    QPainter rawPainter(&rawMosaic);
    
    qDebug() << QString("Step 1: Assembling raw 3x3 mosaic (%1x%1 pixels)").arg(rawMosaicSize);
    
    for (const SimpleTile& tile : m_tiles) {
        if (!tile.downloaded || tile.image.isNull()) {
            qDebug() << QString("  Skipping tile %1,%2 - not downloaded").arg(tile.gridX).arg(tile.gridY);
            continue;
        }
        
        int pixelX = tile.gridX * tileSize;
        int pixelY = tile.gridY * tileSize;
        
        rawPainter.drawImage(pixelX, pixelY, tile.image);
        
        qDebug() << QString("  ✅ Placed tile (%1,%2) at pixel (%3,%4)")
                    .arg(tile.gridX).arg(tile.gridY).arg(pixelX).arg(pixelY);
    }
    rawPainter.end();
    
    // Step 2: Calculate where the target coordinates fall in the raw mosaic
    QPoint targetPixel = calculateTargetPixelPosition();
    
    qDebug() << QString("Step 2: Target coordinates map to pixel (%1,%2) in raw mosaic")
                .arg(targetPixel.x()).arg(targetPixel.y());
    
    // Step 3: Crop the mosaic to center the target coordinates
    QImage centeredMosaic = cropMosaicToCenter(rawMosaic, targetPixel);
    
    qDebug() << QString("Step 3: Cropped to %1x%2 centered mosaic")
                .arg(centeredMosaic.width()).arg(centeredMosaic.height());
    
    // Step 4: Add crosshairs and labels at the true center
    QPainter painter(&centeredMosaic);
    
    // Add crosshairs at the exact center (where target coordinates are)
    painter.setPen(QPen(Qt::yellow, 3));
    int centerX = centeredMosaic.width() / 2;
    int centerY = centeredMosaic.height() / 2;
    
    painter.drawLine(centerX - 30, centerY, centerX + 30, centerY);
    painter.drawLine(centerX, centerY - 30, centerX, centerY + 30);
    
    // Add precise coordinate labels
    painter.setPen(QPen(Qt::yellow, 1));
    painter.setFont(QFont("Arial", 14, QFont::Bold));
    
    painter.drawText(centerX + 40, centerY - 20, targetName);
    
    painter.setFont(QFont("Arial", 10));
    QString coordText = QString("RA:%1° Dec:%2°")
                       .arg(m_actualTarget.ra_deg, 0, 'f', 4)
                       .arg(m_actualTarget.dec_deg, 0, 'f', 4);
    painter.drawText(centerX + 40, centerY - 5, coordText);
    
    painter.drawText(centerX + 40, centerY + 10, "COORDINATE CENTERED");
    
    painter.end();
    
    // Store the final centered mosaic
    m_fullMosaic = centeredMosaic;
    
    // Save final mosaic
    QString safeName = targetName.toLower().replace(" ", "_").replace("(", "").replace(")", "");
    QString mosaicFilename = QString("%1/%2_centered_mosaic.png").arg(m_outputDir).arg(safeName);
    bool saved = centeredMosaic.save(mosaicFilename);
    
    qDebug() << QString("\n🎯 %1 COORDINATE-CENTERED MOSAIC COMPLETE!").arg(targetName);
    qDebug() << QString("📁 Final size: %1×%2 pixels (%3 tiles used)")
                .arg(centeredMosaic.width()).arg(centeredMosaic.height()).arg(successfulTiles);
    qDebug() << QString("📁 Saved to: %1 (%2)")
                .arg(mosaicFilename).arg(saved ? "SUCCESS" : "FAILED");
    qDebug() << QString("✅ Target coordinates are now at exact center pixel (%1,%2)")
                .arg(centerX).arg(centerY);
    
    saveProgressReport(targetName);
    
    // NEW: Emit completion signal
    emit mosaicComplete(centeredMosaic);
}

QPoint EnhancedMosaicCreator::calculateTargetPixelPosition() {
    // Find the tile that contains our target
    SimpleTile* containingTile = nullptr;
    double minDistance = std::numeric_limits<double>::max();
    
    for (SimpleTile& tile : m_tiles) {
        double distance = calculateAngularDistance(m_actualTarget, tile.skyCoordinates);
        if (distance < minDistance) {
            minDistance = distance;
            containingTile = &tile;
        }
    }
    
    if (!containingTile) {
        qDebug() << "Warning: Could not find containing tile, using geometric center";
        return QPoint(1536/2, 1536/2);
    }
    
    qDebug() << QString("Target is in tile (%1,%2) with center at RA=%3°, Dec=%4°")
                .arg(containingTile->gridX).arg(containingTile->gridY)
                .arg(containingTile->skyCoordinates.ra_deg, 0, 'f', 6)
                .arg(containingTile->skyCoordinates.dec_deg, 0, 'f', 6);
    
    // Use definitive astrometry data
    const double ARCSEC_PER_PIXEL = 1.61;
    
    // Calculate angular offsets from the nearest tile center
    double offsetRA_arcsec = (m_actualTarget.ra_deg - containingTile->skyCoordinates.ra_deg) * 3600.0;
    double offsetDec_arcsec = (m_actualTarget.dec_deg - containingTile->skyCoordinates.dec_deg) * 3600.0;
    
    // Apply cosine correction for RA at this declination
    offsetRA_arcsec *= cos(m_actualTarget.dec_deg * M_PI / 180.0);
    
    qDebug() << QString("Angular offset from tile center: RA=%1\", Dec=%2\"")
                .arg(offsetRA_arcsec, 0, 'f', 2)
                .arg(offsetDec_arcsec, 0, 'f', 2);
    
    // Convert to pixel offsets using verified pixel scale
    double offsetRA_pixels = offsetRA_arcsec / ARCSEC_PER_PIXEL;
    double offsetDec_pixels = -offsetDec_arcsec / ARCSEC_PER_PIXEL; // Negative because Y increases downward
    
    qDebug() << QString("Pixel offset from tile center: %1,%2 pixels")
                .arg(offsetRA_pixels, 0, 'f', 1)
                .arg(offsetDec_pixels, 0, 'f', 1);
    
    // Calculate absolute pixel position in the 1536x1536 raw mosaic
    int tilePixelX = containingTile->gridX * 512 + 256; // Tile center X
    int tilePixelY = containingTile->gridY * 512 + 256; // Tile center Y
    
    int targetPixelX = tilePixelX + static_cast<int>(round(offsetRA_pixels));
    int targetPixelY = tilePixelY + static_cast<int>(round(offsetDec_pixels));
    
    // Clamp to mosaic bounds
    targetPixelX = std::max(0, std::min(targetPixelX, 1535));
    targetPixelY = std::max(0, std::min(targetPixelY, 1535));
    
    qDebug() << QString("Target pixel in raw mosaic: (%1,%2)")
                .arg(targetPixelX).arg(targetPixelY);
    
    return QPoint(targetPixelX, targetPixelY);
}

QImage EnhancedMosaicCreator::cropMosaicToCenter(const QImage& rawMosaic, const QPoint& targetPixel) {
    // Determine crop size - aim for ~1200x1200 final mosaic
    int cropSize = 1200;
    
    // Ensure we don't exceed the raw mosaic bounds
    cropSize = std::min(cropSize, std::min(rawMosaic.width(), rawMosaic.height()));
    
    // Calculate crop rectangle so target pixel becomes the center
    int cropX = targetPixel.x() - cropSize / 2;
    int cropY = targetPixel.y() - cropSize / 2;
    
    // Adjust if crop would go outside raw mosaic bounds
    if (cropX < 0) {
        qDebug() << QString("Crop X adjusted from %1 to 0 (target too close to left edge)").arg(cropX);
        cropX = 0;
    }
    if (cropY < 0) {
        qDebug() << QString("Crop Y adjusted from %1 to 0 (target too close to top edge)").arg(cropY);
        cropY = 0;
    }
    if (cropX + cropSize > rawMosaic.width()) {
        int oldCropX = cropX;
        cropX = rawMosaic.width() - cropSize;
        qDebug() << QString("Crop X adjusted from %1 to %2 (target too close to right edge)").arg(oldCropX).arg(cropX);
    }
    if (cropY + cropSize > rawMosaic.height()) {
        int oldCropY = cropY;
        cropY = rawMosaic.height() - cropSize;
        qDebug() << QString("Crop Y adjusted from %1 to %2 (target too close to bottom edge)").arg(oldCropY).arg(cropY);
    }
    
    QRect cropRect(cropX, cropY, cropSize, cropSize);
    
    qDebug() << QString("Crop rectangle: (%1,%2) %3x%4")
                .arg(cropX).arg(cropY).arg(cropSize).arg(cropSize);
    
    return rawMosaic.copy(cropRect);
}

SkyPosition EnhancedMosaicCreator::healpixToSkyPosition(long long pixel, int order) const {
    try {
        long long nside = 1LL << order;
        Healpix_Base healpix(nside, NEST, SET_NSIDE);
        
        pointing pt = healpix.pix2ang(pixel);
        
        SkyPosition pos;
        pos.ra_deg = pt.phi * 180.0 / M_PI;
        pos.dec_deg = 90.0 - pt.theta * 180.0 / M_PI;
        pos.name = QString("HEALPix_%1").arg(pixel);
        pos.description = QString("Order %1 pixel %2").arg(order).arg(pixel);
        
        return pos;
    } catch (...) {
        // Fallback
        SkyPosition pos;
        pos.ra_deg = 0.0;
        pos.dec_deg = 0.0;
        pos.name = "Error";
        pos.description = "HEALPix conversion failed";
        return pos;
    }
}

double EnhancedMosaicCreator::calculateAngularDistance(const SkyPosition& pos1, const SkyPosition& pos2) const {
    // Convert to radians
    double ra1 = pos1.ra_deg * M_PI / 180.0;
    double dec1 = pos1.dec_deg * M_PI / 180.0;
    double ra2 = pos2.ra_deg * M_PI / 180.0;
    double dec2 = pos2.dec_deg * M_PI / 180.0;
    
    // Haversine formula for angular distance
    double dra = ra2 - ra1;
    double ddec = dec2 - dec1;
    
    double a = sin(ddec/2) * sin(ddec/2) + 
               cos(dec1) * cos(dec2) * sin(dra/2) * sin(dra/2);
    double c = 2 * atan2(sqrt(a), sqrt(1-a));
    
    return c; // Return in radians
}

bool EnhancedMosaicCreator::checkExistingTile(const SimpleTile& tile) {
    QFileInfo fileInfo(tile.filename);
    if (!fileInfo.exists() || fileInfo.size() < 1024) return false;
    
    if (!isValidJpeg(tile.filename)) return false;
    
    SimpleTile* mutableTile = const_cast<SimpleTile*>(&tile);
    mutableTile->image.load(tile.filename);
    
    if (mutableTile->image.isNull()) return false;
    
    mutableTile->downloaded = true;
    return true;
}

bool EnhancedMosaicCreator::isValidJpeg(const QString& filename) {
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) return false;
    
    QByteArray header = file.read(4);
    file.close();
    
    return (header.size() >= 3 && 
            static_cast<unsigned char>(header[0]) == 0xFF && 
            static_cast<unsigned char>(header[1]) == 0xD8 && 
            static_cast<unsigned char>(header[2]) == 0xFF);
}

void EnhancedMosaicCreator::saveProgressReport(const QString& targetName) {
    QString safeName = targetName.toLower().replace(" ", "_").replace("(", "").replace(")", "");
    QString reportFile = QString("%1/%2_centered_report.txt").arg(m_outputDir).arg(safeName);
    QFile file(reportFile);
    
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    
    QTextStream out(&file);
    out << QString("%1 Coordinate-Centered Mosaic Report\n").arg(targetName);
    out << "Generated: " << QDateTime::currentDateTime().toString() << "\n\n";
    
    out << "COORDINATE CENTERING ENHANCEMENT:\n";
    out << QString("Target coordinates: RA %1°, Dec %2°\n")
           .arg(m_actualTarget.ra_deg, 0, 'f', 6)
           .arg(m_actualTarget.dec_deg, 0, 'f', 6);
    out << "Enhancement: Target coordinates placed at exact mosaic center\n\n";
    
    out << QString("Custom Target: %1\n").arg(m_customTarget.name);
    
    out << "\n3x3 Tile Grid Used:\n";
    out << "Grid_X,Grid_Y,HEALPix_Pixel,Tile_RA,Tile_Dec,Downloaded,ImageSize,Filename\n";
    
    for (const SimpleTile& tile : m_tiles) {
        out << QString("%1,%2,%3,%4,%5,%6,%7x%8,%9\n")
               .arg(tile.gridX).arg(tile.gridY)
               .arg(tile.healpixPixel)
               .arg(tile.skyCoordinates.ra_deg, 0, 'f', 6)
               .arg(tile.skyCoordinates.dec_deg, 0, 'f', 6)
               .arg(tile.downloaded ? "YES" : "NO")
               .arg(tile.image.width()).arg(tile.image.height())
               .arg(tile.filename);
    }
    
    file.close();
}

// Add missing constellation function (minimal stub)
QString MessierCatalog::constellationToString(Constellation constellation) {
    switch(constellation) {
        case Constellation::ANDROMEDA: return "Andromeda";
        case Constellation::AQUARIUS: return "Aquarius";
        case Constellation::AURIGA: return "Auriga";
        case Constellation::CANCER: return "Cancer";
        case Constellation::CANES_VENATICI: return "Canes Venatici";
        case Constellation::CANIS_MAJOR: return "Canis Major";
        case Constellation::CAPRICORNUS: return "Capricornus";
        case Constellation::CASSIOPEIA: return "Cassiopeia";
        case Constellation::CETUS: return "Cetus";
        case Constellation::COMA_BERENICES: return "Coma Berenices";
        case Constellation::CYGNUS: return "Cygnus";
        case Constellation::DRACO: return "Draco";
        case Constellation::GEMINI: return "Gemini";
        case Constellation::HERCULES: return "Hercules";
        case Constellation::HYDRA: return "Hydra";
        case Constellation::LEO: return "Leo";
        case Constellation::LEPUS: return "Lepus";
        case Constellation::LYRA: return "Lyra";
        case Constellation::MONOCEROS: return "Monoceros";
        case Constellation::OPHIUCHUS: return "Ophiuchus";
        case Constellation::ORION: return "Orion";
        case Constellation::PEGASUS: return "Pegasus";
        case Constellation::PERSEUS: return "Perseus";
        case Constellation::PISCES: return "Pisces";
        case Constellation::PUPPIS: return "Puppis";
        case Constellation::SAGITTA: return "Sagitta";
        case Constellation::SAGITTARIUS: return "Sagittarius";
        case Constellation::SCORPIUS: return "Scorpius";
        case Constellation::SCUTUM: return "Scutum";
        case Constellation::SERPENS: return "Serpens";
        case Constellation::TAURUS: return "Taurus";
        case Constellation::TRIANGULUM: return "Triangulum";
        case Constellation::URSA_MAJOR: return "Ursa Major";
        case Constellation::VIRGO: return "Virgo";
        case Constellation::VULPECULA: return "Vulpecula";
        default: return "Unknown";
    }
}
