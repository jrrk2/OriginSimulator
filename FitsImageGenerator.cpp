#include "FitsImageGenerator.h"

#include <QDateTime>
#include <QString>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {

struct Rng {                                   // xorshift64*; deterministic and fast
    uint64_t s;
    explicit Rng(uint64_t seed) : s(seed ? seed : 0x9E3779B97F4A7C15ull) { for (int i = 0; i < 4; ++i) next(); }
    uint64_t next() { s ^= s >> 12; s ^= s << 25; s ^= s >> 27; return s * 0x2545F4914F6CDD1Dull; }
    double uniform() { return (next() >> 11) * (1.0 / 9007199254740992.0); }
    double normal() { return (uniform() + uniform() + uniform() + uniform() - 2.0) * 1.7320508; }   // sum of four uniforms: unit variance, close enough
};

QByteArray card(const QString& key, const QString& value, const QString& comment = QString())
{
    QString c = QString("%1= %2").arg(key, -8).arg(value, 20);
    if (!comment.isEmpty()) c += " / " + comment;
    return c.leftJustified(80, ' ', true).toLatin1();
}

}   // namespace

QByteArray FitsImageGenerator::generate(const Params& p)
{
    const int W = WIDTH, H = HEIGHT;
    const double black = 4985.0;
    // Bayer GRBG: (row,col) even/even G, even/odd R, odd/even B, odd/odd G. Plane flux ratios of a neutral star: R/G 0.65, B/G 0.67.
    auto gain = [](int x, int y) { return ((x & 1) == (y & 1)) ? 1.0 : ((y & 1) == 0 ? 0.65 : 0.67); };

    const double expo = std::max(p.exposureS, 1e-4) * std::max(p.iso, 1) / 800.0;     // exposure-ISO product relative to 1 s at ISO 800
    const double clear = 1.0 - std::clamp(p.cloud, 0.0, 1.0) * 0.97;
    const double defocus = std::fabs(double(p.focuserPosition - kBestFocus));
    const double sigma = std::sqrt(1.95 * 1.95 + (defocus / 100.0) * (defocus / 100.0));   // gaussian sigma, px (HFR about 1.18 sigma)

    // Pointing-seeded catalogue on a coarse 0.25 degree grid, so a slightly different pointing sees the same stars displaced by the real offset
    // (1.478 arcsec per pixel); the catalogue covers a margin around the frame so stars drift in and out at the edges as they would.
    const double raDeg = p.raRad * 57.29577951, decDeg = p.decRad * 57.29577951, grid = 0.25, pxPerDeg = 3600.0 / 1.478;
    const double gRa = std::round(raDeg / grid) * grid, gDec = std::round(decDeg / grid) * grid;
    const double offX = (raDeg - gRa) * std::cos(decDeg * M_PI / 180.0) * pxPerDeg, offY = -(decDeg - gDec) * pxPerDeg;   // pointing east/north moves the sky west (+x, east is left) / south (+y, down)
    Rng cat(uint64_t(std::llround(gRa * 4 + 1440)) * 1000003ull + uint64_t(std::llround((gDec + 90) * 4)) * 7919ull + 17);
    Rng fr(uint64_t(p.frameNumber) * 2654435761ull + 99);
    const double dx = (fr.uniform() - 0.5) * 3.0 + offX, dy = (fr.uniform() - 0.5) * 3.0 + offY;       // small dither plus pointing offset, px
    const int margin = 700;

    std::vector<float> img(size_t(W) * H);
    // sky: pedestal, rate proportional to exposure x ISO, a gentle gradient, brighter under cloud
    const double skyRate = 18.0 * expo * (1.0 + 4.0 * p.cloud);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            img[size_t(y) * W + x] = float(black + skyRate * gain(x, y) * (1.0 + 0.08 * (double(x) / W - 0.5) + 0.05 * (double(y) / H - 0.5)));

    const int nCat = int(p.stars * double(W + 2 * margin) * (H + 2 * margin) / (double(W) * H));
    if (p.survey && p.survey->size() == size_t(W) * H) {
        // blur the survey to the current seeing (its own resolution is about 0.7 px), then add it through the Bayer filter
        const int x0 = std::max(0, p.surveyFootprint[0]), y0 = std::max(0, p.surveyFootprint[1]), x1 = std::min(W, p.surveyFootprint[2]), y1 = std::min(H, p.surveyFootprint[3]);
        const int w = x1 - x0, h = y1 - y0;
        if (w > 8 && h > 8) {
            std::vector<float> a(size_t(w) * h), b(size_t(w) * h);
            for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) a[size_t(y) * w + x] = (*p.survey)[size_t(y + y0) * W + x + x0];
            const double sb = std::min(12.0, std::sqrt(std::max(0.0, sigma * sigma - 0.7 * 0.7)));
            if (sb > 0.3) {
                const int r = int(std::ceil(3 * sb)); std::vector<float> k(2 * r + 1); double sum = 0;
                for (int i = -r; i <= r; ++i) { k[i + r] = float(std::exp(-i * i / (2 * sb * sb))); sum += k[i + r]; }
                for (float& v : k) v = float(v / sum);
                for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {                  // horizontal pass (edges clamp)
                    double acc = 0; for (int i = -r; i <= r; ++i) acc += k[i + r] * a[size_t(y) * w + std::clamp(x + i, 0, w - 1)];
                    b[size_t(y) * w + x] = float(acc);
                }
                for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {                  // vertical pass
                    double acc = 0; for (int i = -r; i <= r; ++i) acc += k[i + r] * b[size_t(std::clamp(y + i, 0, h - 1)) * w + x];
                    a[size_t(y) * w + x] = float(acc);
                }
            }
            const double K = 3000.0 * expo * clear;                      // linear survey units to ADU: a bright star core 1.0 -> 3000 ADU per (exposure x ISO) unit
            for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) img[size_t(y + y0) * W + x + x0] += float(K * a[size_t(y) * w + x] * gain(x + x0, y + y0));
        }
    }
    const bool real = p.catalogue != nullptr;
    const int nStars = real ? int(p.catalogue->size()) : nCat;
    for (int i = 0; i < nStars; ++i) {
        double sx, sy, flux;
        if (real) {
            const auto& s = (*p.catalogue)[size_t(i)];
            sx = s.x + dx - offX; sy = s.y + dy - offY;                                   // the dither only: the positions are for this pointing already
            flux = 600.0 * expo * clear * std::pow(10.0, -0.4 * (s.g - 12.0)) * s.weight;
        } else {
            sx = cat.uniform() * (W + 2 * margin) - margin + dx; sy = cat.uniform() * (H + 2 * margin) - margin + dy;
            const double mag = 6.0 * std::pow(cat.uniform(), 0.7);                       // 0 (brightest) .. 6 magnitudes fainter
            flux = 9000.0 * expo * clear * std::pow(10.0, -0.4 * mag);                   // total ADU above sky in the green plane
        }
        if (sx < -20 || sx > W + 20 || sy < -20 || sy > H + 20) continue;
        if (p.survey && sx >= p.surveyFootprint[0] && sx < p.surveyFootprint[2] && sy >= p.surveyFootprint[1] && sy < p.surveyFootprint[3]) continue;   // the survey supplies these
        // an elliptical gaussian: the width stays, the length grows with the elongation, along (cos t, -sin t) in pixels (image y runs down)
        const double sMaj = sigma * std::max(1.0, p.elongation), sMin = sigma;
        const double ct = std::cos(p.elongationAngleDeg * M_PI / 180.0), st = -std::sin(p.elongationAngleDeg * M_PI / 180.0);
        const double amp = flux / (2.0 * M_PI * sMaj * sMin);
        const int R = std::min(60, int(std::ceil(4.0 * sMaj)));
        for (int y = std::max(0, int(sy) - R); y <= std::min(H - 1, int(sy) + R); ++y)
            for (int x = std::max(0, int(sx) - R); x <= std::min(W - 1, int(sx) + R); ++x) {
                const double u = (x - sx) * ct + (y - sy) * st, v = -(x - sx) * st + (y - sy) * ct;
                img[size_t(y) * W + x] += float(amp * gain(x, y) * std::exp(-(u * u) / (2 * sMaj * sMaj) - (v * v) / (2 * sMin * sMin)));
            }
    }

    // noise (read noise 8 ADU plus shot noise on the signal above black), then quantise to unsigned 16 bit
    QByteArray data(W * H * 2, 0);
    uint8_t* out = reinterpret_cast<uint8_t*>(data.data());
    for (size_t i = 0; i < img.size(); ++i) {
        const double sig = std::max(0.0, double(img[i]) - black);
        double v = img[i] + fr.normal() * std::sqrt(64.0 + 0.25 * sig);
        v = std::clamp(v, 0.0, 65535.0);
        const int16_t s = int16_t(int(std::lround(v)) - 32768);                         // BZERO 32768
        out[2 * i] = uint8_t(uint16_t(s) >> 8); out[2 * i + 1] = uint8_t(uint16_t(s) & 0xFF);
    }

    QByteArray hdr;
    hdr += card("SIMPLE", "T"); hdr += card("BITPIX", "16"); hdr += card("NAXIS", "2");
    hdr += card("NAXIS1", QString::number(W)); hdr += card("NAXIS2", QString::number(H));
    hdr += card("BZERO", "32768"); hdr += card("BSCALE", "1");
    hdr += card("EXPTIME", QString::number(p.exposureS, 'g', 8), "seconds");
    hdr += card("ISO", QString::number(p.iso)); hdr += card("GAIN", QString::number(p.iso));
    hdr += card("BAYERPAT", "'RGGB    '"); hdr += card("INSTRUME", "'Celestron Origin (simulated)'");
    const double hRa = std::isnan(p.reportedRaRad) ? p.raRad : p.reportedRaRad, hDec = std::isnan(p.reportedDecRad) ? p.decRad : p.reportedDecRad;
    hdr += card("RA", QString::number(hRa * 57.29577951, 'f', 6), "deg"); hdr += card("DEC", QString::number(hDec * 57.29577951, 'f', 6), "deg");
    hdr += card("SIMSTARS", real ? "'GAIA    '" : "'SYNTHETIC'");
    hdr += card("FOCPOS", QString::number(p.focuserPosition)); hdr += card("ASTRSOLV", "F");
    hdr += card("DATE-OBS", "'" + QDateTime::currentDateTimeUtc().toString("yyyy-MM-ddTHH:mm:ss") + "'");
    hdr += QByteArray("END").leftJustified(80, ' ');
    hdr = hdr.leftJustified(((hdr.size() + 2879) / 2880) * 2880, ' ');
    data.append(QByteArray((2880 - data.size() % 2880) % 2880, 0));
    return hdr + data;
}
