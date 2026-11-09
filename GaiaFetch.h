#ifndef GAIAFETCH_H
#define GAIAFETCH_H

#include <QString>

/**
 * @brief Builds the simulator's local star catalogue from ESA's Gaia archive: a one-off maintenance step, run with network access
 * (OriginSimulator --fetch-gaia data/gaia_dr3_g12.bin). The simulator itself never goes online for stars: it reads the file this writes.
 *
 * Gaia DR3 sources with G < 12 (about 3.1 million), fetched in declination bands through the archive's asynchronous TAP service, written as
 * GaiaSky reads them: a 16-byte header ("GAIADR3G", uint32 count, uint32 format 1), then for each star int32 RA and int32 Dec in milliarcseconds
 * and int16 G in millimagnitudes, little-endian, 10 bytes a star.
 *
 * Credit: ESA/Gaia/DPAC. This work has made use of data from the European Space Agency (ESA) mission Gaia (https://www.cosmos.esa.int/gaia), processed
 * by the Gaia Data Processing and Analysis Consortium (DPAC, https://www.cosmos.esa.int/web/gaia/dpac/consortium).
 */
namespace GaiaFetch {
    int run(const QString& outputFile);

    /**
     * The bright stars Gaia leaves out (it saturates on them: nothing brighter than about G 1.7, and gaps to about G 3), from the Hipparcos
     * catalogue (ESA 1997, SP-1200): every star with V < 6, from ESA's archive, where it is public.hipparcos
     * (OriginSimulator --fetch-hipparcos data/hipparcos_v6.bin).
     * Each is carried by its proper motion from Hipparcos' epoch J1991.25 to Gaia DR3's J2016.0, and given a Gaia G from V and B-V by
     * Riello et al. (2021), A&A 649, A3, table C.2: G - V = -0.02704 + 0.01424 (B-V) - 0.2156 (B-V)^2 + 0.01426 (B-V)^3 (G = V without a B-V).
     * Written in the same 10-byte records as the Gaia file, after the header "HIPPARCO", uint32 count, uint32 format 1. GaiaSky adds only the
     * stars its Gaia catalogue lacks.
     */
    int runHipparcos(const QString& outputFile);
}

#endif
