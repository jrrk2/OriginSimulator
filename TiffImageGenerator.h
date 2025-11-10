#ifndef TIFFIMAGEGENERATOR_H
#define TIFFIMAGEGENERATOR_H

#include <QString>
#include <QImage>
#include <tiffio.h>

/**
 * @brief Generates TIFF images matching Origin telescope format
 * 
 * Specifications from real Origin telescope:
 * - Image Width: 3056
 * - Image Length: 2048  
 * - Bits/Sample: 16
 * - Samples/Pixel: 3 (RGB)
 * - Photometric Interpretation: RGB color
 * - Compression: None
 * - Planar Configuration: single image plane
 */
class TiffImageGenerator {
public:
    // Origin telescope sensor dimensions
    static const int IMAGE_WIDTH = 3056;
    static const int IMAGE_HEIGHT = 2048;
    static const int BITS_PER_SAMPLE = 16;
    static const int SAMPLES_PER_PIXEL = 3; // RGB
    
    /**
     * @brief Generate a 16-bit RGB TIFF file matching Origin format
     * @param outputPath Full path where TIFF file should be saved
     * @param sourceImage Optional source image to convert (will be scaled)
     * @return true if successful, false otherwise
     */
    static bool generateOriginFormatTiff(const QString& outputPath, 
                                          const QImage& sourceImage = QImage());
    
    /**
     * @brief Create a synthetic star field for testing
     * @param outputPath Full path where TIFF file should be saved
     * @param numStars Number of stars to generate
     * @return true if successful, false otherwise
     */
    static bool generateSyntheticStarField(const QString& outputPath, int numStars = 100);
    
    /**
     * @brief Convert existing JPG/PNG to 16-bit RGB TIFF
     * @param inputPath Path to source image
     * @param outputPath Path for output TIFF
     * @return true if successful, false otherwise
     */
    static bool convertToOriginTiff(const QString& inputPath, const QString& outputPath);

private:
    /**
     * @brief Write 16-bit RGB TIFF using libtiff
     */
    static bool writeTiff16BitRGB(const QString& outputPath, 
                                   const uint16_t* imageData,
                                   int width, int height);
    
    /**
     * @brief Convert 8-bit RGB to 16-bit RGB (scale from 0-255 to 0-65535)
     */
    static void convert8BitTo16Bit(const uint8_t* src8bit, uint16_t* dst16bit, 
                                     int width, int height);
};

#endif // TIFFIMAGEGENERATOR_H
