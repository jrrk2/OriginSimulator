#include "StellariumDSOOverlay.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <algorithm>
#include <cmath>

namespace {

inline double deg2rad(double d) { return d * M_PI / 180.0; }
inline double rad2deg(double r) { return r * 180.0 / M_PI; }

// Great-circle angular separation between two RA/Dec points (degrees in/out).
double angularSepDeg(double ra1, double dec1, double ra2, double dec2)
{
    const double dRA  = deg2rad(ra1 - ra2);
    const double d1   = deg2rad(dec1);
    const double d2   = deg2rad(dec2);
    double c = std::sin(d1) * std::sin(d2)
             + std::cos(d1) * std::cos(d2) * std::cos(dRA);
    if (c >  1.0) c =  1.0;
    if (c < -1.0) c = -1.0;
    return rad2deg(std::acos(c));
}

} // namespace

StellariumDSOOverlay::StellariumDSOOverlay(const QString& nebulaeDir,
                                           const QString& filterSubstring,
                                           double attenuation)
    : m_nebulaeDir(nebulaeDir)
    , m_attenuation(attenuation)
{
    const QString catPath = QDir(nebulaeDir).filePath("textures.json");
    QFile f(catPath);
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << "StellariumDSOOverlay: cannot open" << catPath;
        return;
    }
    const QByteArray bytes = f.readAll();
    f.close();

    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &err);
    if (doc.isNull()) {
        qWarning() << "StellariumDSOOverlay: JSON parse failed:" << err.errorString();
        return;
    }
    const QJsonObject root = doc.object();
    const QJsonArray  tiles = root.value("subTiles").toArray();

    m_dsos.reserve(tiles.size());
    for (const QJsonValue& tv : tiles) {
        const QJsonObject t = tv.toObject();
        const QString url = t.value("imageUrl").toString();
        const QJsonArray world = t.value("worldCoords").toArray();
        if (url.isEmpty() || world.isEmpty()) continue;
        if (!filterSubstring.isEmpty()
         && !url.contains(filterSubstring, Qt::CaseInsensitive)) continue;
        const QJsonArray quad = world.at(0).toArray();
        if (quad.size() != 4) continue;

        DSO d;
        d.imageUrl     = url;
        d.imageAbsPath = QDir(nebulaeDir).filePath(url);
        d.maxBrightness = t.value("maxBrightness").toDouble(99.0);

        double sumRa = 0, sumDec = 0;
        for (int i = 0; i < 4; ++i) {
            const QJsonArray p = quad.at(i).toArray();
            if (p.size() != 2) { d.cornerRa[0] = -9999; break; }
            d.cornerRa[i]  = p.at(0).toDouble();
            d.cornerDec[i] = p.at(1).toDouble();
            sumRa  += d.cornerRa[i];
            sumDec += d.cornerDec[i];
        }
        if (d.cornerRa[0] < -1000) continue;

        d.centerRa  = sumRa  / 4.0;
        d.centerDec = sumDec / 4.0;
        d.searchRadiusDeg = 0.0;
        for (int i = 0; i < 4; ++i) {
            const double r = angularSepDeg(d.cornerRa[i], d.cornerDec[i],
                                            d.centerRa, d.centerDec);
            if (r > d.searchRadiusDeg) d.searchRadiusDeg = r;
        }
        m_dsos.push_back(std::move(d));
    }

    m_images.resize(m_dsos.size());
    m_loadTried.assign(m_dsos.size(), false);
    m_available = !m_dsos.empty();
    qDebug() << "StellariumDSOOverlay: loaded" << m_dsos.size()
             << "entries from" << catPath
             << (filterSubstring.isEmpty() ? "" : ("filtered by '" + filterSubstring + "'").toUtf8().constData());
}

bool StellariumDSOOverlay::singleCenterCoords(double& raDeg, double& decDeg) const
{
    if (m_dsos.size() != 1) return false;
    raDeg  = m_dsos[0].centerRa;
    decDeg = m_dsos[0].centerDec;
    return true;
}

void StellariumDSOOverlay::paintInto(std::vector<float>& imgR,
                                     std::vector<float>& imgG,
                                     std::vector<float>& imgB,
                                     int width, int height,
                                     double resolution,
                                     double ra_center_deg, double dec_center_deg,
                                     double cameraRotationDeg,
                                     int    stackDepth) const
{
    if (!m_available) return;

    const double dec0Rad = deg2rad(dec_center_deg);
    const double sinDec0 = std::sin(dec0Rad);
    const double cosDec0 = std::cos(dec0Rad);
    const double rotRad  = deg2rad(cameraRotationDeg);
    const double cosRot  = std::cos(rotRad);
    const double sinRot  = std::sin(rotRad);

    // Half-diagonal of field, plus a generous margin, sets the quick reject
    // distance for off-frame DSOs.
    const double halfDiagDeg = 0.5 * std::hypot(double(width), double(height)) * resolution;

    // Per-frame DSO contribution derived from telescope photometry: at the
    // Origin (14cm/QE0.7, 10s, 1.477"/px) a region of surface brightness
    // μ = 17 mag/arcsec² (M51 core) collects ~300 ADU per pixel per single
    // frame. So a PNG-255 pixel on a Stellarium "fully-stacked" tile maps to
    // ~380 ADU per single frame at mag-13 reference (Pogson scaled). The
    // real telescope's stacked master builds linearly with subframe count —
    // we multiply by stackDepth so frame-1 is barely visible and frame-N
    // builds to recognisable structure. The Stellarium PNG itself represents
    // many hours of integrated photons; expect a long stack to approach it.
    constexpr double dsoRefMag       = 13.0;
    constexpr double dsoRefPeakADU   = 380.0;   // peak ADU per PNG-255 px per single frame
    constexpr double dsoMaxPerPixel  = 60000.0; // cap so single pixel can't blow out
    constexpr double dsoMaxFaintness = 17.0;    // skip tiles fainter than this
    constexpr double dsoGamma        = 2.5;     // suppress PNG background pedestal

    const double stackMul = std::max(1, stackDepth);

    // TAN-project an RA/Dec onto the output pixel grid. Returns false if the
    // point is on the far hemisphere (denom <= 0).
    auto projectToPixel = [&](double raDeg, double decDeg, double& px, double& py) -> bool {
        double dRA_deg = raDeg - ra_center_deg;
        while (dRA_deg >  180) dRA_deg -= 360;
        while (dRA_deg < -180) dRA_deg += 360;
        const double dRA    = deg2rad(dRA_deg);
        const double decRad = deg2rad(decDeg);
        const double sinDec = std::sin(decRad);
        const double cosDec = std::cos(decRad);
        const double sinDRA = std::sin(dRA);
        const double cosDRA = std::cos(dRA);
        const double denom  = sinDec0 * sinDec + cosDec0 * cosDec * cosDRA;
        if (denom <= 0.0) return false;
        const double L = cosDec * sinDRA / denom;
        const double M = (cosDec0 * sinDec - sinDec0 * cosDec * cosDRA) / denom;
        const double L_deg = rad2deg(L);
        const double M_deg = rad2deg(M);
        const double Lr = L_deg * cosRot - M_deg * sinRot;
        const double Mr = L_deg * sinRot + M_deg * cosRot;
        px = width  / 2.0 - Lr / resolution;
        py = height / 2.0 - Mr / resolution;
        return true;
    };

    int painted = 0;
    int inFieldButSkipped = 0;

    for (size_t i = 0; i < m_dsos.size(); ++i) {
        const DSO& d = m_dsos[i];

        // Quick reject: DSO too far from pointing centre to overlap the field.
        const double centerSep = angularSepDeg(d.centerRa, d.centerDec,
                                                ra_center_deg, dec_center_deg);
        if (centerSep > halfDiagDeg + d.searchRadiusDeg) continue;

        // Skip DSOs flagged as too faint to be worth rendering.
        if (d.maxBrightness > dsoMaxFaintness) {
            ++inFieldButSkipped;
            continue;
        }

        // Project the four corners. Skip the DSO if any corner is on the far
        // hemisphere — that means it straddles >90° from the pointing, which
        // never happens for in-field DSOs anyway.
        double cpx[4], cpy[4];
        bool allOk = true;
        for (int c = 0; c < 4; ++c) {
            if (!projectToPixel(d.cornerRa[c], d.cornerDec[c], cpx[c], cpy[c])) {
                allOk = false; break;
            }
        }
        if (!allOk) continue;

        // Bounding box in output pixels, clipped to image.
        int minX = int(std::floor(std::min({ cpx[0], cpx[1], cpx[2], cpx[3] })));
        int maxX = int(std::ceil (std::max({ cpx[0], cpx[1], cpx[2], cpx[3] })));
        int minY = int(std::floor(std::min({ cpy[0], cpy[1], cpy[2], cpy[3] })));
        int maxY = int(std::ceil (std::max({ cpy[0], cpy[1], cpy[2], cpy[3] })));
        minX = std::max(0, minX);
        maxX = std::min(width  - 1, maxX);
        minY = std::max(0, minY);
        maxY = std::min(height - 1, maxY);
        if (minX > maxX || minY > maxY) continue;

        // Lazy-load the PNG. Cached for the lifetime of the overlay.
        QImage img;
        {
            std::lock_guard<std::mutex> lk(m_cacheMutex);
            if (!m_loadTried[i]) {
                m_loadTried[i] = true;
                QImage loaded;
                if (loaded.load(d.imageAbsPath)) {
                    // Use the same memory format throughout for fast pixel access.
                    m_images[i] = loaded.convertToFormat(QImage::Format_ARGB32);
                } else {
                    qWarning() << "StellariumDSOOverlay: failed to load" << d.imageAbsPath;
                }
            }
            img = m_images[i];
        }
        if (img.isNull()) continue;

        // Affine inverse: solve for (u, v) in [0,1]^2 such that
        //   (px - cpx[0], py - cpy[0]) = u * (cpx[1]-cpx[0], cpy[1]-cpy[0])
        //                              + v * (cpx[3]-cpx[0], cpy[3]-cpy[0])
        // Stellarium's textureCoords are [[0,0],[1,0],[1,1],[0,1]], so
        // corner 0 maps to texture (0,0), corner 1 to (1,0), corner 3 to (0,1).
        const double dux = cpx[1] - cpx[0];
        const double duy = cpy[1] - cpy[0];
        const double dvx = cpx[3] - cpx[0];
        const double dvy = cpy[3] - cpy[0];
        const double det = dux * dvy - duy * dvx;
        if (std::fabs(det) < 1e-6) continue;

        const int texW = img.width();
        const int texH = img.height();
        const double texWm1 = double(texW - 1);
        const double texHm1 = double(texH - 1);

        // Magnitude → per-pixel peak ADU at PNG value 255, with the runtime
        // attenuation factor folded in and progressive build-up via stackDepth.
        const double scaleADU = std::min(dsoMaxPerPixel,
            stackMul * m_attenuation * dsoRefPeakADU
                     * std::pow(10.0, -0.4 * (d.maxBrightness - dsoRefMag)));

        for (int y = minY; y <= maxY; ++y) {
            const QRgb* rowPtrPrev = nullptr; // bilinear sampling crosses rows
            (void)rowPtrPrev;
            for (int x = minX; x <= maxX; ++x) {
                const double rx = double(x) - cpx[0];
                const double ry = double(y) - cpy[0];
                const double u = ( rx * dvy - ry * dvx) / det;
                const double v = (-rx * duy + ry * dux) / det;
                if (u < 0.0 || u > 1.0 || v < 0.0 || v > 1.0) continue;

                // Bilinear sample of the PNG.
                const double tu = u * texWm1;
                const double tv = v * texHm1;
                const int t0x = int(std::floor(tu));
                const int t0y = int(std::floor(tv));
                const int t1x = std::min(t0x + 1, texW - 1);
                const int t1y = std::min(t0y + 1, texH - 1);
                const double fx = tu - t0x;
                const double fy = tv - t0y;

                const QRgb p00 = img.pixel(t0x, t0y);
                const QRgb p10 = img.pixel(t1x, t0y);
                const QRgb p01 = img.pixel(t0x, t1y);
                const QRgb p11 = img.pixel(t1x, t1y);

                auto blendCh = [&](auto extract) -> double {
                    const double a = (1.0 - fx) * (1.0 - fy);
                    const double b =        fx  * (1.0 - fy);
                    const double c = (1.0 - fx) *        fy;
                    const double e =        fx  *        fy;
                    return extract(p00) * a + extract(p10) * b
                         + extract(p01) * c + extract(p11) * e;
                };
                const double r = blendCh([](QRgb p){ return double(qRed(p));   });
                const double g = blendCh([](QRgb p){ return double(qGreen(p)); });
                const double b = blendCh([](QRgb p){ return double(qBlue(p));  });
                const double a = blendCh([](QRgb p){ return double(qAlpha(p)); });
                if (a < 1.0) continue;

                const double aN = a / 255.0;
                // Gamma compression on the normalised PNG value before scaling
                // — kills the Stellarium tile's background pedestal so it
                // doesn't appear as a rectangular halo around the DSO.
                auto compress = [](double v) {
                    const double n = v / 255.0;
                    return std::pow(n, dsoGamma);
                };
                const float addR = float(compress(r) * scaleADU * aN);
                const float addG = float(compress(g) * scaleADU * aN);
                const float addB = float(compress(b) * scaleADU * aN);
                const int idx = y * width + x;
                imgR[idx] += addR;
                imgG[idx] += addG;
                imgB[idx] += addB;
            }
        }
        ++painted;
        qDebug() << "  DSO" << d.imageUrl
                 << "  centerSep=" << QString::number(centerSep, 'f', 3) << "deg"
                 << "  mag=" << d.maxBrightness
                 << "  scaleADU=" << int(scaleADU);
    }

    if (painted > 0 || inFieldButSkipped > 0)
        qDebug() << "StellariumDSOOverlay: painted" << painted
                 << "DSO(s) into frame (" << inFieldButSkipped << "in-field skipped as too faint)";
}
