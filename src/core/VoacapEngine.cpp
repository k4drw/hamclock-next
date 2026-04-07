/**
 * VoacapEngine.cpp
 *
 * C++ port of the VOACAP/voacapl HF propagation math kernels.
 * Algorithm: ITU-R CCIR (1967) spherical-harmonic ionospheric model.
 *
 * Key Fortran source equivalences (from jawatson/voacapl):
 *   VIRTIM  → computeTimeCoeffs()  : time-harmonic → geographic coefficients
 *   VERSY   → evalVersy()          : geographic Fourier series evaluation
 *   F2VAR   → computeLayerParams() : foF2, hmF2, YmF2, M(3000)F2
 *   EF1VAR  → computeLayerParams() : foE, foF1
 *   CURMUF  → computeMUF()         : oblique MUF for path geometry
 *   PRBMUF  → probMUF() / fnorml() : circuit reliability
 *   BABS    → absorptionDB()       : D-layer absorption estimate
 */

#include "VoacapEngine.h"
#include "ccir_coeffs.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <vector>

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

static constexpr double DEG2RAD = M_PI / 180.0;
static constexpr double RAD2DEG = 180.0 / M_PI;
static constexpr double RE_KM   = 6371.0;   // Earth radius (km)

// Stride within COFION for each variable (Fortran IB(IZ) = first dimension size)
static constexpr int IB[6] = { 5, 7,   5,   13,  9,    9    };

// We only compute variables 3 (F2COF), 4 (FM3COF), 5 (ERCOF).
// Variables 0,1,2 (sporadic-E) are skipped.
static constexpr int IZ_F2  = 3;  // foF2
static constexpr int IZ_M3  = 4;  // M(3000)F2
static constexpr int IZ_ER  = 5;  // E-layer

// ---------------------------------------------------------------------------
// Internal types
// ---------------------------------------------------------------------------

/** Output of computeTimeCoeffs() — the AB array segments for variables 3,4,5 */
struct TimeCoeffs {
    float ab_f2[76];   // AB for IZ_F2 (foF2 geographic Fourier coefficients)
    float ab_m3[49];   // AB for IZ_M3 (M3000 geographic coefficients)
    float ab_er[22];   // AB for IZ_ER (E-layer geographic coefficients)
};

/** Ionospheric layer parameters at a single geographic point */
struct LayerParams {
    double foF2;    // F2 critical frequency (MHz)
    double hmF2;    // F2 virtual height at MUF (km)
    double YmF2;    // F2 semi-thickness (km)
    double M3000;   // M(3000)F2 ratio
    double foE;     // E-layer critical frequency (MHz)
    double gyroMHz; // Electron gyrofrequency (MHz)
};

// ---------------------------------------------------------------------------
// Math utilities
// ---------------------------------------------------------------------------

/** Normal upper-tail probability Q(z) = P(X > z), X ~ N(0,1).
 *  Approximation from VOACAP PRBMUF/FNORML (Abramowitz & Stegun).
 *  Valid for z >= 0; caller negates z for the lower tail. */
static double qnorm(double z) {
    if (z < 0.0) return 1.0 - qnorm(-z);
    double yp = z;
    double q  = 1.0 + yp * (0.196854 + yp * (0.115194 +
                     yp * (0.000344  + yp *  0.019527)));
    q = q * q;
    q = q * q;
    return 0.5 / q;
}

/** P(X <= z) = 1 - Q(z) */
static inline double pnorm(double z) { return 1.0 - qnorm(z); }

// ---------------------------------------------------------------------------
// Geomagnetic model (simplified tilted-dipole approximation, 2020 epoch)
// ---------------------------------------------------------------------------

// Geomagnetic dipole pole position (geographic, 2020 epoch ~ 80.7°N, 72.7°W)
static constexpr double POLE_LAT_RAD = 80.7 * DEG2RAD;
static constexpr double POLE_LON_RAD = (360.0 - 72.7) * DEG2RAD; // 287.3°E

/** Geomagnetic latitude for geographic (lat, lon) in radians. */
static double geomagLat(double lat, double lon) {
    double s = sin(POLE_LAT_RAD) * sin(lat)
             + cos(POLE_LAT_RAD) * cos(lat) * cos(lon - POLE_LON_RAD);
    return asin(std::max(-1.0, std::min(1.0, s)));
}

/** Rawer magnetic dip angle (radians) at geographic (lat, lon) [radians].
 *
 *  The CCIR/voacapl VERSY routine uses the *Rawer* modified dip, not the
 *  standard dip.  From Fortran MAGVAR line 39:
 *    X = ATAN(ATAN(-UNE(1)/SQRT(TMP)) / SQRT(GOB))
 *  i.e. Rawer_dip = atan(standard_dip_radians / sqrt(cos(geographic_lat)))
 *
 *  This amplifies the dip at high geographic latitudes (where cos(lat)→0),
 *  making the CCIR basis functions behave correctly near the poles.
 */
static double magneticDip(double lat, double lon) {
    double glat = geomagLat(lat, lon);
    double dip  = atan2(2.0 * sin(glat), cos(glat));  // standard tilted-dipole dip (radians)
    double gob  = cos(lat);
    if (gob < 1e-6) gob = 1e-6;                        // pole guard (matches Fortran MAGVAR)
    return atan(dip / sqrt(gob));                       // Rawer modification
}

/** Electron gyrofrequency (MHz) at geographic latitude (radians).
 *  B_dipole = 0.305 Gauss * sqrt(1 + 3*sin²(geomag_lat)); GYZ = 2.8*B_Gauss */
static double gyroFreqMHz(double lat, double lon) {
    double glat = geomagLat(lat, lon);
    double sinG = sin(glat);
    double B_gauss = 0.305 * sqrt(1.0 + 3.0 * sinG * sinG);
    return 2.8 * B_gauss;
}

// ---------------------------------------------------------------------------
// Solar geometry
// ---------------------------------------------------------------------------

/** Solar zenith angle cosine at (lat, lon) [radians] for UTC decimal hour.
 *  Uses day-of-year from month (1-12) approximation. */
static double cosSZA(double lat, double lon, double utcHour, int month) {
    // Solar declination: approximate by month mid-point day-of-year
    static const int midDay[12] = {16,46,75,106,136,167,197,228,259,289,320,350};
    double doy = midDay[std::max(0, std::min(11, month - 1))];
    double declination = -23.45 * DEG2RAD * cos(2.0 * M_PI * (doy + 10.0) / 365.0);
    // Hour angle at geographic longitude (east positive)
    double H = (utcHour - 12.0) * 15.0 * DEG2RAD + lon;
    return sin(lat) * sin(declination) + cos(lat) * cos(declination) * cos(H);
}

// ---------------------------------------------------------------------------
// VIRTIM equivalent: evaluate time Fourier harmonics → AB geographic coefficients
// ---------------------------------------------------------------------------

/**
 * Build SSN-interpolated flat COFION array for a single variable IZ.
 * Returns a flat array of size IB[iz] * n_geo_terms.
 * SSN-interpolated as: cof = (cof_low*(100-ssn) + cof_high*ssn) / 100
 * (for F2/FM3/ER; CCIR model uses 100-SSN/SSN weighting).
 */
static void buildCofion(float *cofion_iz, int iz, int month, float ssn) {
    // iz: 3=F2COF, 4=FM3COF, 5=ERCOF  (0-indexed variable index)
    const float *low, *high;
    int n;
    if (iz == IZ_F2) {
        low  = CCIR::f2cof[month][0];
        high = CCIR::f2cof[month][1];
        n = 13 * 76;  // IB[3]*n_geo_terms = 13*76
    } else if (iz == IZ_M3) {
        low  = CCIR::fm3cof[month][0];
        high = CCIR::fm3cof[month][1];
        n = 9 * 49;
    } else { // IZ_ER
        low  = CCIR::ercof[month][0];
        high = CCIR::ercof[month][1];
        n = 9 * 22;
    }
    float ssn100 = 100.0f - ssn;
    for (int i = 0; i < n; ++i)
        cofion_iz[i] = (low[i] * ssn100 + high[i] * ssn) * 0.01f;
}

/**
 * VIRTIM equivalent: compute time-harmonic AB coefficients for one variable.
 *
 * cofion_iz: flat array starting at COFION[IA[iz]], size IB[iz]*n_geo_terms.
 *            Layout: cofion_iz[i + jb*IB[iz]] = COFION(IA[iz]+1+i+jb*IB[iz]) in Fortran.
 *            = the (i+1)-th harmonic coeff for geographic term (jb+1).
 * ab_out:    output array of size n_geo_terms (= ikim[iz][8]+1).
 * utcHour:   UTC decimal hour.
 */
static void virtimVar(float *ab_out, const float *cofion_iz, int iz, float utcHour) {
    int ib = IB[iz];
    int n_geo = CCIR::ikim[iz][8] + 1;     // IKIM(9,IZ)+1
    int n_harm = CCIR::ikim[iz][9];        // IKIM(10,IZ)

    // TIME = (15*GMT - 180) * DEG2RAD
    double time_rad = (15.0 * utcHour - 180.0) * DEG2RAD;
    double c1 = cos(time_rad), s1 = sin(time_rad);

    // Build C[k], S[k] for k=1..n_harm via recurrence
    double C[9], S[9];
    C[1] = c1; S[1] = s1;
    for (int k = 2; k <= n_harm; ++k) {
        C[k] = c1 * C[k-1] - s1 * S[k-1];
        S[k] = c1 * S[k-1] + s1 * C[k-1];
    }

    // For each geographic term jb (0-indexed):
    // ab[jb] = cofion[jb*ib] + sum_k( S[k]*cofion[jb*ib+2k-1] + C[k]*cofion[jb*ib+2k] )
    for (int jb = 0; jb < n_geo; ++jb) {
        const float *p = cofion_iz + (size_t)jb * ib;
        float val = p[0];   // DC term: COFION(ISUBB)
        for (int k = 1; k <= n_harm; ++k) {
            // ISUCC = ISUBB + 2*KA - 1  (Fortran 1-indexed)
            // In 0-indexed: p[2k-1] = sin coeff, p[2k] = cos coeff
            val += (float)S[k] * p[2*k - 1] + (float)C[k] * p[2*k];
        }
        ab_out[jb] = val;
    }
}

/**
 * Compute time coefficients for all three needed variables.
 * Called once per map computation (time/month/SSN don't change across pixels).
 */
static TimeCoeffs computeTimeCoeffs(int month, float ssn, float utcHour) {
    TimeCoeffs tc;
    float cofion_f2[13 * 76];
    float cofion_m3[9 * 49];
    float cofion_er[9 * 22];

    buildCofion(cofion_f2, IZ_F2, month, ssn);
    buildCofion(cofion_m3, IZ_M3, month, ssn);
    buildCofion(cofion_er, IZ_ER, month, ssn);

    virtimVar(tc.ab_f2, cofion_f2, IZ_F2, utcHour);
    virtimVar(tc.ab_m3, cofion_m3, IZ_M3, utcHour);
    virtimVar(tc.ab_er, cofion_er, IZ_ER, utcHour);

    return tc;
}

// ---------------------------------------------------------------------------
// VERSY equivalent: geographic Fourier series evaluation
// ---------------------------------------------------------------------------

/**
 * Evaluate geographic Fourier series GAMMA(IZ) at a geographic point.
 *
 * iz:         0-indexed variable (3=foF2, 4=M3000, 5=E-layer)
 * ab:         AB array for this variable (size = ikim[iz][8]+1)
 * dip_x:      For iz=3,4: magnetic dip angle (radians).
 *             For iz=5:   geographic latitude (radians).
 * lat:        Geographic latitude (radians) — used for GOB=cos(lat) in all cases.
 * east_lon:   East longitude (radians).
 */
static double evalVersy(int iz, const float *ab,
                        double dip_x, double lat, double east_lon) {
    // IKIM indices (0-indexed):
    // ikim[iz][0] = K (number of pure sin(X) powers, Fortran IKIM(1,IZ))
    // ikim[iz][1..8] = geographic term limits for each longitude harmonic
    // ikim[iz][8] = total geographic terms - 1
    // ikim[iz][9] = number of time harmonics (not used here, already folded into AB)

    // Use 1-indexed G[] matching Fortran convention (G[1]..G[76])
    double G[78] = {};  // 1-indexed: G[1..max_terms], max=76

    double SX  = sin(dip_x);
    double GOB = cos(lat);         // cos(geographic_lat), = cos(CENLAT)

    // Normalize east longitude to [0, 2π)
    double Y = east_lon;
    if (Y < 0.0) Y += 2.0 * M_PI;

    int K = CCIR::ikim[iz][0];    // number of pure SX powers after G[1]

    // G[1] = 1 (constant term)
    // G[2] = SX, G[3] = SX², ..., G[K+1] = SX^K
    G[1] = 1.0;
    G[2] = SX;
    if (K >= 2) {
        for (int ka = 2; ka <= K; ++ka)
            G[ka + 1] = SX * G[ka];
    }

    // Now handle longitude Fourier terms
    int KDIF = CCIR::ikim[iz][1] - K;
    if (KDIF > 0) {
        int JG = 1;
        double CX = GOB;
        double T  = Y;

        while (true) {
            // Fortran: KK = IKIM(JG, IZ) + 4
            int KK = CCIR::ikim[iz][JG - 1] + 4;
            G[KK - 2] = CX * cos(T);
            G[KK - 1] = CX * sin(T);

            // Fortran: LO = IKIM(JG+1, IZ)
            int LO = CCIR::ikim[iz][JG];

            // Loop filling G[KK..LO+1] via SX multiplication
            if (LO >= KK) {
                for (int ka = KK; ka <= LO; ka += 2) {
                    if (ka <= LO)   G[ka]     = SX * G[ka - 2];
                    G[ka + 1] = SX * G[ka - 1];
                }
            }

            // Check whether to continue
            if (JG >= 8) break;
            KDIF = CCIR::ikim[iz][JG + 1] - LO;
            if (KDIF <= 0) break;

            CX *= GOB;
            JG++;
            T = (double)JG * Y;
        }
    }

    // Final summation: GAMMA = sum_{jb=1}^{I} G[jb] * AB[jb-1]
    int n_terms = CCIR::ikim[iz][8] + 1;  // I = IKIM(9,IZ)+1
    double gamma = 0.0;
    for (int jb = 1; jb <= n_terms; ++jb)
        gamma += G[jb] * (double)ab[jb - 1];

    return gamma;
}

// ---------------------------------------------------------------------------
// Layer parameter computation (F2VAR + EF1VAR equivalents)
// ---------------------------------------------------------------------------

/**
 * Compute ionospheric layer parameters at geographic (lat, lon) [radians].
 *
 * Uses pre-computed time coefficients from computeTimeCoeffs().
 * ssn:     sunspot number (0-200)
 * utcHour: UTC decimal hour
 * month:   1-12
 */
static LayerParams computeLayerParams(double lat, double lon,
                                      const TimeCoeffs &tc,
                                      float ssn, float utcHour, int month) {
    LayerParams lp{};

    double dip = magneticDip(lat, lon);
    lp.gyroMHz = gyroFreqMHz(lat, lon);

    // --- F2 layer (F2VAR) ---
    double gamma4 = evalVersy(IZ_F2, tc.ab_f2, dip, lat, lon);  // foF2 base
    double gamma5 = evalVersy(IZ_M3, tc.ab_m3, dip, lat, lon);  // M(3000)F2

    // foF2 with half-gyro correction, PSC(3)=1.0 (simplified)
    lp.foF2  = gamma4 + 0.5 * lp.gyroMHz;
    lp.foF2  = std::max(0.5, lp.foF2);

    lp.M3000 = gamma5;
    if (lp.M3000 < 1.0) lp.M3000 = 1.0;

    // hmF2 from M(3000)F2: virtual height at 0.834*foF2
    lp.hmF2 = 1490.0 / lp.M3000 - 176.0;
    lp.hmF2 = std::max(100.0, std::min(600.0, lp.hmF2));

    // YmF2 = hmF2 / RAT where typical RAT ~ 5
    lp.YmF2 = lp.hmF2 / 5.0;

    // --- E layer (EF1VAR simplified) ---
    // Use VERSY with geographic latitude (not dip) for ERCOF
    double gamma6 = evalVersy(IZ_ER, tc.ab_er, lat, lat, lon);

    // Solar zenith correction for foE
    double czx = cosSZA(lat, lon, utcHour, month);
    if (czx < 0.0) czx = 0.0;
    // foE = GAMMA(6) * cos(SZA)^0.3 (Chapman-like factor, PSC(1) simplified)
    // At night czx≈0 → foE≈0; at noon czx≈1 → foE≈gamma6
    lp.foE = gamma6 * pow(czx + 0.001, 0.3);
    lp.foE = std::max(0.1, lp.foE);

    return lp;
}

// ---------------------------------------------------------------------------
// Path geometry
// ---------------------------------------------------------------------------

/** Great-circle distance (km) between two points in radians. */
static double greatCircleKm(double lat1, double lon1, double lat2, double lon2) {
    double dlat = lat2 - lat1;
    double dlon = lon2 - lon1;
    double a = sin(dlat * 0.5) * sin(dlat * 0.5)
             + cos(lat1) * cos(lat2) * sin(dlon * 0.5) * sin(dlon * 0.5);
    double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
    return RE_KM * c;
}

/** Interpolate a point fraction f along the great circle from (lat1,lon1) to (lat2,lon2).
 *  f=0 → start, f=1 → end, f=0.5 → midpoint. All in radians. */
static void greatCirclePoint(double lat1, double lon1,
                             double lat2, double lon2,
                             double f,
                             double &outLat, double &outLon) {
    double dlat = lat2 - lat1;
    double dlon = lon2 - lon1;
    double a = sin(dlat * 0.5) * sin(dlat * 0.5)
             + cos(lat1) * cos(lat2) * sin(dlon * 0.5) * sin(dlon * 0.5);
    double d = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));  // angular distance (radians)
    if (d < 1e-10) { outLat = lat1; outLon = lon1; return; }

    double sinD = sin(d);
    double A = sin((1.0 - f) * d) / sinD;
    double B = sin(f * d) / sinD;
    double x = A * cos(lat1) * cos(lon1) + B * cos(lat2) * cos(lon2);
    double y = A * cos(lat1) * sin(lon1) + B * cos(lat2) * sin(lon2);
    double z = A * sin(lat1)             + B * sin(lat2);
    outLat = atan2(z, sqrt(x * x + y * y));
    outLon = atan2(y, x);
}

// ---------------------------------------------------------------------------
// MUF computation (CURMUF simplified)
// ---------------------------------------------------------------------------

/**
 * Compute F2 MUF for an oblique path.
 *
 * Uses parabolic layer model: MUF = foF2 × sec(elevation_angle)
 * where sec(el) = sqrt(1 + (d_half / hmF2)²) for a 1-hop path.
 *
 * Returns the MUF in MHz and the elevation angle in degrees.
 */
static double computeF2MUF(double gcd_km, const LayerParams &lp, double *elev_deg_out) {
    if (gcd_km < 1.0) {
        // NVIS: MUF ≈ foF2
        if (elev_deg_out) *elev_deg_out = 80.0;
        return lp.foF2;
    }

    // 1-hop parameters
    double halfDist1 = gcd_km / 2.0;
    double sec1 = sqrt(1.0 + (halfDist1 / lp.hmF2) * (halfDist1 / lp.hmF2));
    double muf1 = lp.foF2 * sec1;
    double elev1 = atan2(lp.hmF2, halfDist1) * RAD2DEG;

    // 2-hop: each hop covers gcd/2; sec for each hop is sqrt(1+(gcd/4/hmF2)²)
    double halfDist2 = gcd_km / 4.0;
    double sec2 = sqrt(1.0 + (halfDist2 / lp.hmF2) * (halfDist2 / lp.hmF2));
    double muf2 = lp.foF2 * sec2;    // 2-hop MUF is lower (gentler angle per hop)
    double elev2 = atan2(lp.hmF2, halfDist2) * RAD2DEG;

    // Max single-hop skip distance (km) — roughly 2 * sqrt(2*RE*hmF2)
    double maxSkip1 = 2.0 * sqrt(2.0 * RE_KM * lp.hmF2);

    if (gcd_km <= maxSkip1) {
        // 1-hop is geometrically feasible
        if (elev_deg_out) *elev_deg_out = elev1;
        return muf1;
    } else {
        // 2-hop (or could do more, but 2 is sufficient for most world-map paths)
        if (elev_deg_out) *elev_deg_out = elev2;
        return muf2;
    }
}

// ---------------------------------------------------------------------------
// Absorption estimate (BABS simplified)
// ---------------------------------------------------------------------------

/**
 * D-layer absorption estimate (dB) — simplified BABS.
 * Uses the CCIR D-layer index proportional to cos(SZA)^0.75.
 */
static double absorptionDB(double gcd_km, double freqMHz, const LayerParams &lp,
                           double lat, double lon, float utcHour, int month) {
    double czx = cosSZA(lat, lon, utcHour, month);
    if (czx <= 0.0) return 0.0;   // Night → no D-layer absorption

    // ACAV absorption index: proportional to sqrt(cos(SZA))
    double acav = sqrt(czx);
    double fsum = freqMHz + lp.gyroMHz;
    if (fsum < 1.0) fsum = 1.0;

    // BABS: AB = 677.2 * acav / (10.2 + fsum^1.98)
    double denom = 10.2 + pow(fsum, 1.98);
    double absPerHop = 677.2 * acav / denom;

    // Path factor: number of hops × elevation correction
    double maxSkip = 2.0 * sqrt(2.0 * RE_KM * lp.hmF2);
    int nHops = (gcd_km <= maxSkip) ? 1 : 2;
    double halfDist = gcd_km / (2.0 * nHops);
    double secEl = sqrt(1.0 + (halfDist / lp.hmF2) * (halfDist / lp.hmF2));
    double pathFactor = (double)nHops * secEl;

    return absPerHop * pathFactor;
}

// ---------------------------------------------------------------------------
// Reliability computation (PRBMUF + SNR, VOACAP REL = P(MUF) × P(SNR))
// ---------------------------------------------------------------------------

/** Mode decoding advantage over SSB (dB). Does NOT include power — power is in P_tx_dBm. */
static double modeAdvantageDB(const std::string &mode) {
    if      (mode == "CW")    return 16.0;
    else if (mode == "FT8")   return 12.0;
    else if (mode == "FT4")   return 10.0;
    else if (mode == "JT65")  return 15.0;
    else if (mode == "WSPR")  return 25.0;
    else if (mode == "PSK31") return 14.0;
    else if (mode == "RTTY")  return  5.0;
    else if (mode == "AM")    return -6.0;
    else if (mode == "FM")    return -3.0;
    return 0.0;  // SSB baseline
}

/**
 * Circuit reliability (%) — VOACAP model: REL = P(MUF) × P(SNR).
 *
 * P(MUF):  Probability the ionosphere supports the path at this frequency.
 *          Uses log-normal MUF distribution, sigma_muf ≈ 8% of MUF (CCIR).
 *
 * P(SNR):  Probability received SNR ≥ required SNR for the mode.
 *          Path budget: Tx power − free-space loss − D-layer absorption
 *          − atmospheric noise (ITU-R P.372) − required mode SNR.
 *          Variability sigma_snr ≈ 9 dB (HF path fading, VOACAP default).
 *
 * freqMHz:    Operating frequency
 * muf:        Computed F2 MUF (MHz)
 * gcdKm:      Path great-circle distance (km)
 * absDB:      D-layer absorption (dB)
 * txPowerW:   Transmitter power (watts)
 * mode:       Mode string ("SSB", "CW", "FT8", etc.)
 */
static double computeReliability(double freqMHz, double muf,
                                 double gcdKm,   double absDB,
                                 double txPowerW, const std::string &mode) {
    if (muf <= 0.0 || freqMHz <= 0.0) return 0.0;

    // --- P(MUF): ionospheric path support probability ---
    // MUF distribution sigma ≈ 8% of MUF (CCIR/VOACAP standard)
    double sigma_muf = 0.08;
    double z_muf = (muf - freqMHz) / (sigma_muf * muf);
    double p_muf = pnorm(z_muf);

    // --- P(SNR): signal-to-noise probability ---
    // Free-space path loss (dB): L = 32.4 + 20log(f_MHz) + 20log(d_km)
    double d_km = std::max(1.0, gcdKm);
    double L_fs = 32.4 + 20.0 * log10(freqMHz) + 20.0 * log10(d_km);

    // TX power in dBm
    double P_tx_dBm = 10.0 * log10(std::max(0.01, txPowerW)) + 30.0;

    // Atmospheric noise floor in SSB bandwidth (3 kHz), ITU-R P.372 simplified:
    //   Fa ≈ 76.8 - 27.7*log10(f_MHz)  [atmospheric noise factor, dB above thermal]
    //   kTB (3kHz) = -174 + 34.8 = -139.2 dBm
    //   N_floor = -139.2 + Fa dBm
    double Fa = 76.8 - 27.7 * log10(freqMHz);
    double N_floor = -139.2 + Fa;           // dBm, in 3 kHz bandwidth

    // Effective antenna gain: assume typical amateur installation ~3 dB over isotropic
    // (dipole at non-optimal angle, accounts for ground losses etc.)
    static constexpr double ANT_GAIN_DB = 3.0;

    // Required SNR for SSB baseline ≈ 10 dB (ITU-R circuit quality threshold)
    static constexpr double REQ_SNR_SSB = 10.0;

    // Signal margin = available_signal - noise - required_SNR + mode_advantage
    double sm = (P_tx_dBm + ANT_GAIN_DB)
              - L_fs - absDB
              - N_floor - REQ_SNR_SSB
              + modeAdvantageDB(mode);

    // P(SNR): HF path SNR is log-normally distributed, sigma ≈ 9 dB (VOACAP)
    static constexpr double sigma_snr = 9.0;
    double p_snr = pnorm(sm / sigma_snr);

    return std::max(0.0, std::min(100.0, p_muf * p_snr * 100.0));
}

// ---------------------------------------------------------------------------
// Main grid computation
// ---------------------------------------------------------------------------

std::vector<float> VoacapEngine::generateGrid(const PropPathParams &params,
                                              const SolarData &sw,
                                              const class IonosondeProvider * /*iono*/,
                                              int outputType) {
    std::vector<float> grid(MAP_W * MAP_H, 0.0f);

    // --- Determine time parameters ---
    time_t now_t = time(nullptr);
    struct tm *utc = gmtime(&now_t);
    int month   = utc->tm_mon + 1;          // 1-12
    float utcHour = (float)utc->tm_hour
                  + (float)utc->tm_min / 60.0f
                  + (float)utc->tm_sec / 3600.0f;

    // Clamp SSN to [0, 200] for interpolation
    float ssn = (float)std::max(0, std::min(200, sw.sunspot_number));
    // CCIR data is for SSN 0-100; clamp to table range
    float ssn_clamp = std::min(ssn, 100.0f);

    // --- Compute time-varying coefficients (once for entire grid) ---
    TimeCoeffs tc = computeTimeCoeffs(month - 1, ssn_clamp, utcHour);

    // TX position in radians
    double txLat = params.txLat * DEG2RAD;
    double txLon = params.txLon * DEG2RAD;
    double freqMHz = params.mhz;
    double minToaDeg = (params.toa > 0) ? (double)params.toa : 3.0;
    bool longPath = (params.path == 1);

    // --- Per-pixel computation ---
    for (int py = 0; py < MAP_H; ++py) {
        for (int px = 0; px < MAP_W; ++px) {
            // Map pixel → geographic coordinates
            // MAP: 660×330, lon ∈ [-180,180), lat ∈ [90,-90) top→bottom
            double rxLon = ((double)px / MAP_W - 0.5) * 2.0 * M_PI;   // -π..π
            double rxLat = (0.5 - (double)py / MAP_H) * M_PI;          // π/2..-π/2

            // Path distance and midpoint, respecting short/long path choice.
            // Long-path midpoint is the antipode of the short-path midpoint.
            double shortKm = greatCircleKm(txLat, txLon, rxLat, rxLon);
            double gcd_km  = longPath ? (40075.0 - shortKm) : shortKm;

            if (gcd_km < 10.0) {
                if (outputType == 1) grid[py * MAP_W + px] = 100.0f;
                else if (outputType == 2) grid[py * MAP_W + px] = 88.0f;
                else grid[py * MAP_W + px] = 35.0f;
                continue;
            }

            double midLat, midLon;
            greatCirclePoint(txLat, txLon, rxLat, rxLon, 0.5, midLat, midLon);
            if (longPath) {
                midLat = -midLat;
                midLon += M_PI;
                if (midLon > M_PI) midLon -= 2.0 * M_PI;
            }

            LayerParams lp = computeLayerParams(midLat, midLon, tc,
                                                ssn_clamp, utcHour, month);
            double elev;
            double muf = computeF2MUF(gcd_km, lp, &elev);

            float val = 0.0f;
            if (outputType == 0) {
                // VOACAP MUF: oblique path MUF from DE to this pixel.
                // Filtered by minimum TOA (antenna elevation constraint).
                if (elev >= minToaDeg)
                    val = (float)std::max(0.0, std::min(35.0, muf));
            } else if (outputType == 1) {
                // Reliability: full circuit model, TOA-filtered.
                if (elev >= minToaDeg) {
                    double absDB = absorptionDB(gcd_km, freqMHz, lp,
                                                midLat, midLon, utcHour, month);
                    val = (float)computeReliability(freqMHz, muf, gcd_km, absDB,
                                                   params.watts, params.mode);
                }
            } else {
                // TOA output: elevation angle to this pixel (degrees)
                val = (float)std::max(0.0, std::min(88.0, elev));
            }

            grid[py * MAP_W + px] = val;
        }
    }

    return grid;
}

double VoacapEngine::pathReliability(double freqMHz, double distKm,
                                     double midLatDeg, double midLonDeg,
                                     int utcHour, int month,
                                     double ssn, double watts,
                                     const std::string &mode,
                                     double minToaDeg) {
    if (distKm < 10.0)
        return 100.0;

    float ssn_clamp = (float)std::max(0.0, std::min(100.0, ssn));
    TimeCoeffs tc = computeTimeCoeffs(month - 1, ssn_clamp, (float)utcHour);

    double midLat = midLatDeg * DEG2RAD;
    double midLon = midLonDeg * DEG2RAD;

    LayerParams lp = computeLayerParams(midLat, midLon, tc,
                                        ssn_clamp, (float)utcHour, month);
    double elev;
    double muf = computeF2MUF(distKm, lp, &elev);

    if (elev < minToaDeg)
        return 0.0;

    double absDB = absorptionDB(distKm, freqMHz, lp, midLat, midLon,
                                (double)utcHour, month);
    return computeReliability(freqMHz, muf, distKm, absDB, watts, mode);
}
