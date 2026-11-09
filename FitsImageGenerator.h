#ifndef FITSIMAGEGENERATOR_H
#define FITSIMAGEGENERATOR_H

#include <QByteArray>
#include <cmath>
#include <vector>

/**
 * @brief Raw 16-bit Bayer FITS snapshots, as the current Origin firmware (1.5.x) delivers when BitDepth=16 and Debayer=false.
 *
 * Format matched to real frames: 3056 x 2048, BITPIX 16 with BZERO 32768 (unsigned data stored as signed), black level about 4985 ADU,
 * Bayer layout GRBG as stored (greens on the main diagonal; the BAYERPAT keyword says RGGB, as the real header does).
 *
 * The star field depends only on the pointing, so successive frames of one target contain the same stars (with a small dither, so stacking
 * and registration have something to do). Star size depends on the focuser position: sharpest at kBestFocus, wider on either side, so a focus
 * sweep finds a minimum (with a survey image, by blurring it). Brightness follows exposure x ISO. A cloud factor (0..1) dims the stars and raises the sky.
 */
class FitsImageGenerator {
public:
    static const int WIDTH = 3056;
    static const int HEIGHT = 2048;
    static const int kBestFocus = 18900;       // focuser position of best focus

    struct Params {
        double raRad = 0, decRad = 0;
        double exposureS = 1.0;
        int iso = 800;
        int focuserPosition = kBestFocus;
        int frameNumber = 0;                   // dither and noise seed
        double cloud = 0.0;                    // 0 clear .. 1 opaque
        int stars = 300;
        // Optional survey image (see CelestronOriginSimulator --survey): linear, background-subtracted luminance at the sensor plate scale, WIDTH x HEIGHT
        // floats, 1.0 = a bright star core. It is blurred to the focus-dependent seeing and replaces the synthetic stars inside `surveyFootprint`
        // (x0, y0, x1, y1; the area the survey tiles cover); outside it the synthetic stars remain.
        const std::vector<float>* survey = nullptr;
        int surveyFootprint[4] = {0, 0, 0, 0};
        // Optional real stars (see GaiaSky): pixel positions for the true pointing and Gaia G magnitudes. When given they replace the synthetic
        // stars, so that plate solvers solve the frame where the tube points. G 12 gives about 600 ADU per (exposure x ISO) unit.
        // weight: the share of the exposure a position holds; a star drawn at several positions through the exposure trails (field rotation, sidereal drift)
        struct PixelStar { double x, y, g, weight = 1.0; };
        const std::vector<PixelStar>* catalogue = nullptr;
        // What the header's RA/DEC report: the firmware's idea of the pointing, as on a real frame (NaN: the pointing above). The truth is not in the header.
        double reportedRaRad = NAN, reportedDecRad = NAN;
        // Star shape: length over width (1 round; the tube still ringing after a slew makes them oval) and the long axis's angle, from the image's right toward up
        double elongation = 1.0, elongationAngleDeg = 0.0;
    };

    static QByteArray generate(const Params& p);
};

#endif
