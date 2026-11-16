# Star catalogue

`gaia_dr3_g12.bin`: the 3.1 million Gaia DR3 sources brighter than G = 12, for the simulated frames (`GaiaSky`), so that plate solvers solve them where the simulated tube really points. The simulator reads this file and never goes online for stars.

It was fetched once from ESA's Gaia archive with the simulator's own maintenance option:

    OriginSimulator --fetch-gaia data/gaia_dr3_g12.bin

which runs, in 15-degree declination bands, `SELECT ra, dec, phot_g_mean_mag FROM gaiadr3.gaia_source WHERE phot_g_mean_mag < 12` (epoch J2016.0 positions; proper motion is not applied, which matters for a handful of fast stars only).

Format (little-endian): `GAIADR3G`, uint32 count, uint32 format (1); then per star int32 RA and int32 Dec in milliarcseconds and int16 G in millimagnitudes.

## Credit

This work has made use of data from the European Space Agency (ESA) mission Gaia (https://www.cosmos.esa.int/gaia), processed by the Gaia Data Processing and Analysis Consortium (DPAC, https://www.cosmos.esa.int/web/gaia/dpac/consortium). Funding for the DPAC has been provided by national institutions, in particular the institutions participating in the Gaia Multilateral Agreement.

Gaia data: ESA/Gaia/DPAC. Gaia Collaboration, Vallenari et al. (2023), "Gaia Data Release 3: Summary of the content and survey properties", A&A 674, A1. ESA's terms (https://www.cosmos.esa.int/web/gaia-users/license) make the data free to use with this credit; this file is an unaltered selection of three columns.

# Bright stars

`hipparcos_v6.bin`: the 4992 Hipparcos stars brighter than V = 6, for the bright stars Gaia leaves out. Gaia saturates on them, so DR3 has nothing brighter than about G = 1.7 and gaps to about G = 3: no Vega, Sirius or Capella. `GaiaSky` adds a Hipparcos star only when the Gaia file has no star within 3 arcseconds and 1 magnitude of it (98 stars).

It was fetched once from ESA's archive, which holds the Hipparcos catalogue as `public.hipparcos`, with:

    OriginSimulator --fetch-hipparcos data/hipparcos_v6.bin

which runs `SELECT hip, ra, de, pmra, pmde, vmag, b_v FROM public.hipparcos WHERE vmag < 6`. Each star is moved by its proper motion from the Hipparcos epoch, J1991.25, to Gaia DR3's, J2016.0. It is given a Gaia G from V and B-V by Riello et al. (2021), A&A 649, A3, table C.2: G - V = -0.02704 + 0.01424 (B-V) - 0.2156 (B-V)^2 + 0.01426 (B-V)^3.

Same format as the Gaia file, with `HIPPARCO` in place of `GAIADR3G`.

Credit: ESA (1997), The Hipparcos and Tycho Catalogues, ESA SP-1200.
