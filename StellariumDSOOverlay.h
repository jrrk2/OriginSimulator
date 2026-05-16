#ifndef STELLARIUMDSOOVERLAY_H
#define STELLARIUMDSOOVERLAY_H

#include <QString>
#include <QImage>
#include <vector>
#include <mutex>

// Reads Stellarium's nebulae/default/textures.json catalogue and renders the
// PNG nebula/galaxy/cluster images into the simulator's float RGB buffers
// using the same TAN projection + camera-rotation geometry as the star
// renderer, so DSOs sit under the same sky pipeline (Rayleigh, moonglow,
// stretch) as the stars overlaid on them.
//
// Pictures are loaded lazily from disk on first use and cached for the
// lifetime of the overlay object. Brightness is driven by Stellarium's
// per-tile `maxBrightness` field — a magnitude — using a Pogson scale so
// bright Messier objects saturate while faint NGCs sit just above sky.
class StellariumDSOOverlay
{
public:
    // nebulaeDir is the directory containing textures.json plus the PNGs
    // (e.g. /Applications/Stellarium.app/Contents/Resources/nebulae/default).
    // filterSubstring, if non-empty, restricts the catalogue to entries whose
    // imageUrl contains the substring (case-insensitive). Used by the
    // --dso=<name> startup workaround so the App only ever sees one DSO.
    // attenuation multiplies the per-pixel ADU contribution of each DSO. 1.0
    // is the calibrated default; <1 dims, >1 brightens. Useful for tuning
    // particular targets without rebuilding (set via --dso-attenuation=N).
    explicit StellariumDSOOverlay(const QString& nebulaeDir,
                                  const QString& filterSubstring = QString(),
                                  double attenuation = 1.0);

    bool isAvailable() const { return m_available; }
    int  catalogueSize() const { return int(m_dsos.size()); }

    // For the single-DSO startup mode: returns the center RA/Dec (degrees) of
    // the sole entry if exactly one DSO is loaded; false otherwise.
    bool singleCenterCoords(double& raDeg, double& decDeg) const;

    // Add DSO contributions to the float RGB buffers. The projection
    // parameters mirror those used by GaiaStarFieldRenderer::renderField so
    // alignment is exact.
    // stackDepth multiplies the per-pixel ADU contribution, mirroring the way
    // a real telescope's accumulated stack signal grows linearly with the
    // number of subs. Frame 1 → very faint, frame N → fully visible.
    void paintInto(std::vector<float>& imgR,
                   std::vector<float>& imgG,
                   std::vector<float>& imgB,
                   int width, int height,
                   double resolution_deg_per_pix,
                   double ra_center_deg, double dec_center_deg,
                   double cameraRotationDeg,
                   int    stackDepth = 1) const;

private:
    struct DSO {
        QString imageUrl;          // filename in nebulaeDir
        QString imageAbsPath;
        double  cornerRa[4]{};     // degrees, BL/BR/TR/TL per Stellarium texture order
        double  cornerDec[4]{};
        double  maxBrightness = 99.0;
        double  centerRa = 0.0;    // mean of corners, for fast in-field check
        double  centerDec = 0.0;
        double  searchRadiusDeg = 0.0; // max corner-to-center distance
    };

    QString m_nebulaeDir;
    std::vector<DSO> m_dsos;
    double  m_attenuation = 1.0;
    bool    m_available = false;

    // Lazy image cache, keyed by index into m_dsos. mutable+mutex so paintInto
    // can be const.
    mutable std::vector<QImage> m_images;
    mutable std::vector<bool>   m_loadTried;
    mutable std::mutex          m_cacheMutex;
};

#endif // STELLARIUMDSOOVERLAY_H
