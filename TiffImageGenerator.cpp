#include "TiffImageGenerator.h"
#include <QDebug>
#include <QFile>
#include <QDateTime>
#include <cstring>
#include <cstdlib>
#include <cmath>

bool TiffImageGenerator::generateOriginFormatTiff(const QString& outputPath, 
                                                    const QImage& sourceImage) {
    qDebug() << "Generating Origin-format TIFF:" << outputPath;
    
    // Allocate 16-bit RGB buffer (3 channels, 16-bit each)
    size_t pixelCount = IMAGE_WIDTH * IMAGE_HEIGHT;
    size_t bufferSize = pixelCount * SAMPLES_PER_PIXEL * sizeof(uint16_t);
    uint16_t* imageData = (uint16_t*)malloc(bufferSize);
    
    if (!imageData) {
        qDebug() << "Failed to allocate image buffer";
        return false;
    }
    
    // Initialize with default data
    if (sourceImage.isNull()) {
        // Create a simple gradient for testing
        for (int y = 0; y < IMAGE_HEIGHT; y++) {
            for (int x = 0; x < IMAGE_WIDTH; x++) {
                size_t idx = (y * IMAGE_WIDTH + x) * SAMPLES_PER_PIXEL;
                
                // Create a subtle gradient (16-bit range: 0-65535)
                uint16_t value = (uint16_t)((x * 65535.0) / IMAGE_WIDTH);
                
                imageData[idx + 0] = value;      // R
                imageData[idx + 1] = value / 2;  // G
                imageData[idx + 2] = value / 3;  // B
            }
        }
    } else {
        // Scale and convert source image
        QImage scaled = sourceImage.scaled(IMAGE_WIDTH, IMAGE_HEIGHT, 
                                           Qt::IgnoreAspectRatio, 
                                           Qt::SmoothTransformation);
        
        for (int y = 0; y < IMAGE_HEIGHT; y++) {
            for (int x = 0; x < IMAGE_WIDTH; x++) {
                QRgb pixel = scaled.pixel(x, y);
                size_t idx = (y * IMAGE_WIDTH + x) * SAMPLES_PER_PIXEL;
                
                // Convert 8-bit RGB to 16-bit RGB (scale 0-255 to 0-65535)
                imageData[idx + 0] = (uint16_t)((qRed(pixel) * 65535) / 255);
                imageData[idx + 1] = (uint16_t)((qGreen(pixel) * 65535) / 255);
                imageData[idx + 2] = (uint16_t)((qBlue(pixel) * 65535) / 255);
            }
        }
    }
    
    // Write TIFF
    bool success = writeTiff16BitRGB(outputPath, imageData, IMAGE_WIDTH, IMAGE_HEIGHT);
    
    free(imageData);
    
    if (success) {
        qDebug() << "Successfully generated TIFF:" << outputPath;
    } else {
        qDebug() << "Failed to write TIFF:" << outputPath;
    }
    
    return success;
}

bool TiffImageGenerator::generateSyntheticStarField(const QString& outputPath, int numStars) {
    qDebug() << "Generating synthetic star field with" << numStars << "stars";
    
    // Allocate 16-bit RGB buffer
    size_t pixelCount = IMAGE_WIDTH * IMAGE_HEIGHT;
    size_t bufferSize = pixelCount * SAMPLES_PER_PIXEL * sizeof(uint16_t);
    uint16_t* imageData = (uint16_t*)malloc(bufferSize);
    
    if (!imageData) {
        qDebug() << "Failed to allocate image buffer";
        return false;
    }
    
    // Initialize with dark sky background (slight noise)
    for (size_t i = 0; i < pixelCount * SAMPLES_PER_PIXEL; i++) {
        imageData[i] = rand() % 500;  // Dark background with noise
    }
    
    // Add stars
    srand(QDateTime::currentMSecsSinceEpoch());
    
    for (int i = 0; i < numStars; i++) {
        int x = rand() % IMAGE_WIDTH;
        int y = rand() % IMAGE_HEIGHT;
        uint16_t brightness = 20000 + (rand() % 45535);  // Bright stars
        int radius = 1 + (rand() % 3);  // Star size
        
        // Draw star with Gaussian-like profile
        for (int dy = -radius; dy <= radius; dy++) {
            for (int dx = -radius; dx <= radius; dx++) {
                int px = x + dx;
                int py = y + dy;
                
                if (px >= 0 && px < IMAGE_WIDTH && py >= 0 && py < IMAGE_HEIGHT) {
                    float dist = sqrt(dx*dx + dy*dy);
                    float intensity = exp(-dist * dist / (radius * radius));
                    
                    size_t idx = (py * IMAGE_WIDTH + px) * SAMPLES_PER_PIXEL;
                    uint16_t starValue = (uint16_t)(brightness * intensity);
                    
                    // Add to all channels (white star)
                    imageData[idx + 0] = std::min(65535, (int)imageData[idx + 0] + starValue);
                    imageData[idx + 1] = std::min(65535, (int)imageData[idx + 1] + starValue);
                    imageData[idx + 2] = std::min(65535, (int)imageData[idx + 2] + starValue);
                }
            }
        }
    }
    
    // Write TIFF
    bool success = writeTiff16BitRGB(outputPath, imageData, IMAGE_WIDTH, IMAGE_HEIGHT);
    
    free(imageData);
    
    return success;
}

bool TiffImageGenerator::convertToOriginTiff(const QString& inputPath, 
                                              const QString& outputPath) {
    QImage sourceImage(inputPath);
    
    if (sourceImage.isNull()) {
        qDebug() << "Failed to load source image:" << inputPath;
        return false;
    }
    
    qDebug() << "Converting" << inputPath << "to Origin-format TIFF";
    return generateOriginFormatTiff(outputPath, sourceImage);
}

bool TiffImageGenerator::writeTiff16BitRGB(const QString& outputPath, 
                                            const uint16_t* imageData,
                                            int width, int height) {
    // Open TIFF file for writing
    TIFF* tif = TIFFOpen(outputPath.toUtf8().constData(), "w");
    if (!tif) {
        qDebug() << "Failed to open TIFF file for writing:" << outputPath;
        return false;
    }
    
    // Set TIFF tags to match Origin telescope format
    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, width);
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, height);
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 16);
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 3);
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
    TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);  // Single image plane
    TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(tif, width * 3 * 2));
    
    // Set software tag to match Origin
    QString softwareTag = QString("OriginSimulator %1")
                          .arg(QDateTime::currentDateTime().toString("MM-dd-yyyy HH:mm"));
    TIFFSetField(tif, TIFFTAG_SOFTWARE, softwareTag.toUtf8().constData());
    
    // Write image data row by row
    size_t rowSize = width * SAMPLES_PER_PIXEL * sizeof(uint16_t);
    
    for (int row = 0; row < height; row++) {
        const uint16_t* rowData = imageData + (row * width * SAMPLES_PER_PIXEL);
        
        if (TIFFWriteScanline(tif, (void*)rowData, row, 0) < 0) {
            qDebug() << "Failed to write TIFF scanline" << row;
            TIFFClose(tif);
            return false;
        }
    }
    
    // Close the TIFF file
    TIFFClose(tif);
    
    return true;
}
