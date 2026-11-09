#include "GaiaSky.h"

#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace {
constexpr int kRecord = 38;
inline int cellOf(double raDeg, double decDeg)
{
    const int band = std::clamp(int(std::floor(decDeg + 90.0)), 0, 179);
    const int bin = std::clamp(int(std::floor(raDeg)), 0, 359);
    return band * 360 + bin;
}
}   // namespace

GaiaSky& GaiaSky::instance()
{
    static GaiaSky sky;
    return sky;
}

#ifndef ORIGIN_SIM_SOURCE_DIR
#define ORIGIN_SIM_SOURCE_DIR "."
#endif

GaiaSky::GaiaSky() : m_cells(180 * 360)
{
    const QString env = qEnvironmentVariable("ORIGIN_SIM_GAIA");
    if (env == "none") return;
    if (!env.isEmpty()) {
        if (QFileInfo(env).isFile()) loadFile(env); else load(env);
    } else {
        bool loaded = false;
        for (const QString& file : { QCoreApplication::applicationDirPath() + "/data/gaia_dr3_g12.bin", QString(ORIGIN_SIM_SOURCE_DIR) + "/data/gaia_dr3_g12.bin" })
            if (!loaded && QFile::exists(file)) loaded = loadFile(file);
        if (!loaded) load(QDir::homePath() + "/.skychart/catalog/gaia/gaia1");
    }
    if (m_count) addBright();
}

QByteArray GaiaSky::readCatalogue(const QString& file, const char* magic, uint32_t& count)
{
    // GaiaFetch's format: an 8-byte name, uint32 count, uint32 format 1, then int32 RA mas, int32 Dec mas, int16 G mmag per star (little-endian)
    QFile f(file);
    count = 0;
    if (!f.open(QIODevice::ReadOnly)) return {};
    QByteArray data = f.readAll();
    uint32_t format = 0;
    if (data.size() < 16 || !data.startsWith(magic)) { qWarning() << "GaiaSky: not a catalogue file:" << file; return {}; }
    std::memcpy(&count, data.constData() + 8, 4); std::memcpy(&format, data.constData() + 12, 4);
    if (format != 1 || data.size() < 16 + qsizetype(count) * 10) { qWarning() << "GaiaSky: unexpected format or size in" << file; count = 0; return {}; }
    return data;
}

void GaiaSky::addBright()
{
    for (const QString& file : { QCoreApplication::applicationDirPath() + "/data/hipparcos_v6.bin", QString(ORIGIN_SIM_SOURCE_DIR) + "/data/hipparcos_v6.bin" }) {
        uint32_t count = 0;
        const QByteArray data = QFile::exists(file) ? readCatalogue(file, "HIPPARCO", count) : QByteArray();
        if (!count) continue;
        std::vector<Star> missing;
        const char* p = data.constData() + 16;
        for (uint32_t i = 0; i < count; ++i, p += 10) {
            int32_t ra, dec; int16_t g;
            std::memcpy(&ra, p, 4); std::memcpy(&dec, p + 4, 4); std::memcpy(&g, p + 8, 2);
            const Star s{ float(ra / 3.6e6), float(dec / 3.6e6), float(g / 1000.0) };
            const auto near = cone(s.raDeg, s.decDeg, 3.0 / 3600.0, 99);
            if (std::none_of(near.begin(), near.end(), [&](const Star& n) { return std::fabs(n.g - s.g) < 1.0; })) missing.push_back(s);
        }
        for (const Star& s : missing) add(s.raDeg, s.decDeg, s.g);
        m_bright = missing.size();
        qDebug() << "GaiaSky:" << m_bright << "bright stars Gaia lacks added from" << file << "(Hipparcos, ESA 1997)";
        return;
    }
    qWarning() << "GaiaSky: no data/hipparcos_v6.bin: the brightest stars (Vega, Sirius, ...) are missing from the frames";
}

void GaiaSky::add(double raDeg, double decDeg, double g)
{
    if (decDeg < -90 || decDeg > 90 || raDeg < 0 || raDeg >= 360) return;
    m_cells[cellOf(raDeg, decDeg)].push_back({ float(raDeg), float(decDeg), float(g) });
    ++m_count;
}

bool GaiaSky::loadFile(const QString& file)
{
    QElapsedTimer timer; timer.start();
    uint32_t count = 0;
    const QByteArray data = readCatalogue(file, "GAIADR3G", count);
    if (!count) return false;
    const char* p = data.constData() + 16;
    for (uint32_t i = 0; i < count; ++i, p += 10) {
        int32_t ra, dec; int16_t g;
        std::memcpy(&ra, p, 4); std::memcpy(&dec, p + 4, 4); std::memcpy(&g, p + 8, 2);
        add(ra / 3.6e6, dec / 3.6e6, g / 1000.0);
    }
    m_dir = file;
    qDebug() << "GaiaSky:" << m_count << "stars from" << file << "in" << timer.elapsed() << "ms (Gaia DR3, credit ESA/Gaia/DPAC)";
    return true;
}

void GaiaSky::load(const QString& dir)
{
    QElapsedTimer timer; timer.start();
    if (!QFile::exists(dir + "/gaia.hdr")) {
        qWarning() << "GaiaSky: no Gaia catalogue at" << dir << "- frames use synthetic stars, which plate solvers cannot solve";
        return;
    }
    m_dir = dir;
    QDirIterator it(dir, { "gaia*.dat" }, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QFile f(it.next());
        if (!f.open(QIODevice::ReadOnly)) continue;
        const QByteArray data = f.readAll();
        const char* p = data.constData();
        for (qsizetype off = 0; off + kRecord <= data.size(); off += kRecord) {
            int32_t ra, dec; int16_t g;
            std::memcpy(&ra, p + off, 4); std::memcpy(&dec, p + off + 4, 4); std::memcpy(&g, p + off + 16, 2);
            add(ra / 3.6e6, (dec - 324000000) / 3.6e6, g / 1000.0);
        }
    }
    qDebug() << "GaiaSky:" << m_count << "stars from" << dir << "in" << timer.elapsed() << "ms";
}

std::vector<GaiaSky::Star> GaiaSky::cone(double raDeg, double decDeg, double radiusDeg, double gMax) const
{
    std::vector<Star> out;
    if (!m_count) return out;
    const double d2r = M_PI / 180.0;
    const double cosR = std::cos(radiusDeg * d2r);
    const double sd = std::sin(decDeg * d2r), cd = std::cos(decDeg * d2r);
    const int b0 = std::max(0, int(std::floor(decDeg - radiusDeg + 90))), b1 = std::min(179, int(std::floor(decDeg + radiusDeg + 90)));
    for (int band = b0; band <= b1; ++band) {
        const double edge = std::max(std::fabs(band - 90.0), std::fabs(band + 1 - 90.0));        // the band's edge furthest from the equator
        const double c = std::cos(std::min(89.9, edge) * d2r);
        const bool all = edge > 89 || radiusDeg / c >= 180;
        const int span = all ? 180 : int(std::ceil(radiusDeg / c)) + 1;
        for (int k = -span; k <= span && (!all || k < 180); ++k) {
            const int bin = ((int(std::floor(raDeg)) + k) % 360 + 360) % 360;
            for (const Star& s : m_cells[band * 360 + bin]) {
                if (s.g > gMax) continue;
                const double cosSep = sd * std::sin(s.decDeg * d2r) + cd * std::cos(s.decDeg * d2r) * std::cos((s.raDeg - raDeg) * d2r);
                if (cosSep >= cosR) out.push_back(s);
            }
        }
    }
    return out;
}
