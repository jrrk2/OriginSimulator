#ifndef GAIASKY_H
#define GAIASKY_H

#include <QString>
#include <vector>

/**
 * @brief Real stars for the simulated frames, so that plate solvers (astrometry.net's solve-field and its 42xx indices) solve them where the tube
 * really points.
 *
 * Local files only: the simulator never goes online for stars. In order, the first found:
 *  - ORIGIN_SIM_GAIA: a catalogue file, a Cartes du Ciel folder, or "none" (synthetic stars);
 *  - data/gaia_dr3_g12.bin beside the executable, then in the source tree: Gaia DR3 to G 12 (3.1 million stars), fetched once from ESA's archive by
 *    OriginSimulator --fetch-gaia (GaiaFetch), committed with the source. Credit: ESA/Gaia/DPAC;
 *  - the Gaia DR3 catalogue as Cartes du Ciel installs it (~/.skychart/catalog/gaia/gaia1, G to 12): 7.5-degree zone folders of gaiaNNN.dat files,
 *    fixed 38-byte records (little-endian: RA int32 milliarcseconds, Dec int32 milliarcseconds + 90 degrees, source id int64, G, BP, RP int16
 *    millimagnitudes, ...), as gaia.hdr describes them.
 * The stars are kept in a one-degree grid for quick cone searches.
 */
class GaiaSky {
public:
    struct Star { float raDeg, decDeg, g; };

    // ORIGIN_SIM_GAIA, or ~/.skychart/catalog/gaia/gaia1; ORIGIN_SIM_GAIA=none turns it off
    static GaiaSky& instance();

    bool available() const { return m_count > 0; }
    size_t count() const { return m_count; }
    QString source() const { return m_dir; }

    // stars within radiusDeg of (raDeg, decDeg), brighter than gMax
    std::vector<Star> cone(double raDeg, double decDeg, double radiusDeg, double gMax = 13.0) const;

private:
    GaiaSky();
    void load(const QString& dir);
    bool loadFile(const QString& file);
    void add(double raDeg, double decDeg, double g);
    std::vector<std::vector<Star>> m_cells;          // 180 dec bands x 360 ra bins
    size_t m_count = 0;
    QString m_dir;
};

#endif
