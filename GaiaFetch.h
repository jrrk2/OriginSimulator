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
}

#endif
