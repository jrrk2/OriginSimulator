# Star catalogue

`gaia_dr3_g12.bin`: the 3.1 million Gaia DR3 sources brighter than G = 12, for the simulated frames (`GaiaSky`), so that plate solvers solve them where the simulated tube really points. The simulator reads this file and never goes online for stars.

It was fetched once from ESA's Gaia archive with the simulator's own maintenance option:

    OriginSimulator --fetch-gaia data/gaia_dr3_g12.bin

which runs, in 15-degree declination bands, `SELECT ra, dec, phot_g_mean_mag FROM gaiadr3.gaia_source WHERE phot_g_mean_mag < 12` (epoch J2016.0 positions; proper motion is not applied, which matters for a handful of fast stars only).

Format (little-endian): `GAIADR3G`, uint32 count, uint32 format (1); then per star int32 RA and int32 Dec in milliarcseconds and int16 G in millimagnitudes.

## Credit

This work has made use of data from the European Space Agency (ESA) mission Gaia (https://www.cosmos.esa.int/gaia), processed by the Gaia Data Processing and Analysis Consortium (DPAC, https://www.cosmos.esa.int/web/gaia/dpac/consortium). Funding for the DPAC has been provided by national institutions, in particular the institutions participating in the Gaia Multilateral Agreement.

Gaia data: ESA/Gaia/DPAC. Gaia Collaboration, Vallenari et al. (2023), "Gaia Data Release 3: Summary of the content and survey properties", A&A 674, A1. ESA's terms (https://www.cosmos.esa.int/web/gaia-users/license) make the data free to use with this credit; this file is an unaltered selection of three columns.
