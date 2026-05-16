#ifndef GAIASTARFIELDRENDERER_H
#define GAIASTARFIELDRENDERER_H

#include <QObject>
#include <QByteArray>
#include <QString>
#include <vector>
#include <cstdint>

class StellariumDSOOverlay;

// Environmental conditions affecting the sky background. Defaults model a
// suburban Bortle-5 site at zenith with no Moon and no atmospheric scattering.
struct SkyConditions {
    double altitudeDeg     = 90.0;  // pointing altitude (zenith=90), drives airmass / Rayleigh
    double moonSepDeg      = 180.0; // angular separation between pointing and Moon
    double moonPhase       = 0.0;   // illuminated fraction 0..1 (0=new, 1=full)
    bool   moonUp          = false; // is the Moon above the horizon?
    bool   rayleighEnabled = false; // off by default: airmass brightening can
                                    // wash out the App's star detector at low
                                    // altitudes. Enable via --rayleigh.
    int    bortleClass     = 5;     // 1=pristine sky, 9=inner city. Adds a
                                    // sky-photon pedestal + shot noise on top
                                    // of the sensor bias/amp/dark baseline.
    double exposureSec     = 10.0;  // shutter time; sky photons scale linearly
};

class GaiaStarFieldRenderer : public QObject {
    Q_OBJECT

public:
    explicit GaiaStarFieldRenderer(QObject *parent = nullptr);

    // Render a star field centered on (ra_deg, dec_deg) at given pixel scale.
    // sky drives Rayleigh sky-brightness scaling and moonglow contribution.
    // cameraRotationDeg rotates the projected star field around the pointing
    // centre — used to simulate alt-az field rotation: as time advances on an
    // alt-az mount, the camera frame rotates relative to the sky.
    // exposureRotationDeg is the *additional* rotation that accumulates
    // during the shutter-open period; nonzero values produce arc trails per
    // star, with length proportional to distance from the pointing centre.
    // dsoOverlay, if non-null, paints Stellarium's nebula/galaxy/cluster
    // images into the same scene before the TIFF is encoded.
    // Returns a 16-bit RGB TIFF in a QByteArray, or empty on failure.
    QByteArray renderField(double ra_deg, double dec_deg,
                           int width, int height,
                           double pixscale_arcsec,
                           const SkyConditions& sky = {},
                           double cameraRotationDeg = 0.0,
                           double exposureRotationDeg = 0.0,
                           const StellariumDSOOverlay* dsoOverlay = nullptr,
                           int    stackDepth = 1);

    bool isAvailable() const { return m_available; }

private:
    bool m_available = false;
    QString m_dbDir;

    static void bpRpToRGB(float bp_rp, float &r, float &g, float &b);
    static QByteArray write16BitRGBTiff(int w, int h,
                                         const std::vector<uint16_t> &r,
                                         const std::vector<uint16_t> &g,
                                         const std::vector<uint16_t> &b);
};

#endif // GAIASTARFIELDRENDERER_H
