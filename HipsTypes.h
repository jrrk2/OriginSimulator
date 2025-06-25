// HipsTypes.h - Common types and utilities for HiPS client
#ifndef HIPSTYPES_H
#define HIPSTYPES_H

#include <QString>
#include <QList>
#include <QDateTime>
#include <memory>

// Forward declarations
class HipsTile;

// Constants
constexpr double PI = 3.14159265358979323846;
constexpr double DEG_TO_RAD = PI / 180.0;
constexpr double RAD_TO_DEG = 180.0 / PI;

// Sky Coordinates Class
class SkyCoordinates {
public:
    double ra_deg;
    double dec_deg;
    double fov_deg;
    int hips_order;
    bool is_valid;
    QString validation_message;
    
    // Viewport dimensions (matching JavaScript clients)
    int viewport_width;
    int viewport_height;
    
    // Constructors
    SkyCoordinates(double ra, double dec, double fov, int width = 1024, int height = 768);
    
    // Methods
    void validateAndNormalize();
    int calculateOptimalOrder();
    QString toString() const;
    bool isInKnownCoverage() const;
    
private:
    void validateRubinCoverage();
};

// HiPS Tile Class
class HipsTile {
public:
    int order;
    long long healpix_pixel;
    bool is_loaded;
    QByteArray data;
    QString content_type;
    QString error_message;
    QDateTime fetch_time;
    
    // Constructors
    HipsTile(int ord, long long pixel);
    
    // Methods
    QString getFilename(const QString& survey_name) const;
    bool saveToFile(const QString& filepath) const;
    bool isValid() const { return is_loaded && !data.isEmpty(); }
};

// HiPS Utilities Class
class HipsUtils {
public:
    // Core HEALPix algorithms
    static long long radecToHealpixNested(double ra_deg, double dec_deg, int order);
    static QList<long long> calculateViewportTiles(const SkyCoordinates& coords);
    static QList<long long> getNeighborPixels(long long central_pixel, int order, int radius);
    
    // Helper functions
    static long long ringToNested(long long ring_pixel, long long nside);
    static double calculatePixelAngularSize(int order);
    static int calculateOptimalOrderForViewport(double fov_deg, int viewport_width, int viewport_height);
    
    // Coordinate utilities
    static void normalizeCoordinates(double& ra_deg, double& dec_deg);
    static bool isValidCoordinate(double ra_deg, double dec_deg);
};

#endif // HIPSTYPES_H
