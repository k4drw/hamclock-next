#include "PropEngine.h"
#include "../services/IonosondeProvider.h"
#include <algorithm>
#include <cmath>
#include <map>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Mode advantages relative to SSB (0 dB)
static const std::map<std::string, double> MODE_ADVANTAGE_DB = {
    {"CW", 16.0},   {"FT8", 12.0},  {"FT4", 10.0}, {"JT65", 15.0},
    {"WSPR", 25.0}, {"SSB", 0.0},   {"AM", -6.0},  {"FM", -3.0},
    {"RTTY", 5.0},  {"PSK31", 14.0}};

double PropEngine::calculateSignalMargin(const std::string &mode,
                                         double watts) {
  double modeAdv = 0.0;
  auto it = MODE_ADVANTAGE_DB.find(mode);
  if (it != MODE_ADVANTAGE_DB.end()) {
    modeAdv = it->second;
  }

  double p = std::max(0.01, watts);
  double powerOffset = 10.0 * std::log10(p / 100.0);
  return modeAdv + powerOffset;
}

double PropEngine::calculateMUF(double distKm, double midLat, double midLon,
                                double hour, double sfi, double ssn,
                                const InterpolatedIonosonde &ionoData) {
  double muf3000 = 0.0;

  // Prefer real-time ionosonde data
  if (ionoData.mufd.has_value()) {
    muf3000 = ionoData.mufd.value();
  } else if (ionoData.stationsUsed > 0 && ionoData.foF2 > 0) {
    muf3000 = ionoData.foF2 * ionoData.md;
  }

  // Solar Model Fallback
  if (muf3000 == 0.0) {
    double hourFactor = 1.0 + 0.4 * std::cos((hour - 14.0) * M_PI / 12.0);
    double latFactor = 1.0 - std::abs(midLat) / 150.0;
    double foF2_est = 0.9 * std::sqrt(ssn + 15.0) * hourFactor * latFactor;
    double M = 3.0; // Assume standard factor
    muf3000 = foF2_est * M;
  }

  // Convert MUF(3000) to MUF(Distance)
  if (distKm < 3000.0) {
    return muf3000 * std::sqrt(distKm / 3000.0);
  } else {
    return muf3000 * (1.0 + 0.15 * std::log10(distKm / 3000.0));
  }
}

double PropEngine::calculateLUF(double distKm, double midLat, double hour,
                                double sfi, double kIndex) {
  double pathFactor = std::sqrt(distKm / 1000.0);
  double solarFactor = std::sqrt(sfi);

  double zenithAngle = std::abs(hour - 12.0) * 15.0; // degrees from noon
  double zenithRad = zenithAngle * M_PI / 180.0;
  double diurnalFactor = std::pow(std::max(0.1, std::cos(zenithRad)), 0.5);

  double stormFactor = 1.0 + (kIndex * 0.1);

  double baseLuf =
      2.0 * pathFactor * solarFactor * diurnalFactor * stormFactor / 10.0;

  // Nighttime reduction
  if (hour < 6.0 || hour > 18.0) {
    baseLuf *= 0.3;
  }

  return std::max(1.0, baseLuf);
}

double PropEngine::calculateReliability(double freqMhz, double distKm,
                                        double midLat, double midLon,
                                        double hour, double sfi, double ssn,
                                        double kIndex,
                                        const InterpolatedIonosonde &ionoData,
                                        double currentHour,
                                        double signalMarginDb) {
  // Adjust ionosonde data for hour diff (if needed, simplified port)
  // For now, assume ionoData is "current" enough or has been adjusted.
  // Full VOACAP logic scales foF2 based on hour diff, but PropEngine usually
  // calls this with "current" iono data, so we can skip complex scaling unless
  // doing forecasting.

  double muf = calculateMUF(distKm, midLat, midLon, hour, sfi, ssn, ionoData);
  double luf = calculateLUF(distKm, midLat, hour, sfi, kIndex);

  double effectiveMuf = muf * (1.0 + signalMarginDb * 0.012);
  double effectiveLuf = luf * std::max(0.1, 1.0 - signalMarginDb * 0.008);

  double rel = 0.0;

  if (freqMhz > effectiveMuf * 1.1) {
    rel = std::max(0.0, 30.0 - (freqMhz - effectiveMuf) * 5.0);
  } else if (freqMhz > effectiveMuf) {
    rel = 30.0 + (effectiveMuf * 1.1 - freqMhz) / (effectiveMuf * 0.1) * 20.0;
  } else if (freqMhz < effectiveLuf * 0.8) {
    rel = std::max(0.0, 20.0 - (effectiveLuf - freqMhz) * 10.0);
  } else if (freqMhz < effectiveLuf) {
    rel = 20.0 + (freqMhz - effectiveLuf * 0.8) / (effectiveLuf * 0.2) * 30.0;
  } else {
    // OWF / FOT logic
    double rRange = effectiveMuf - effectiveLuf;
    if (rRange <= 0) {
      rel = 30.0;
    } else {
      double pos = (freqMhz - effectiveLuf) / rRange;
      double optimal = 0.75;
      if (pos < optimal) {
        rel = 50.0 + (pos / optimal) * 45.0;
      } else {
        rel = 95.0 - ((pos - optimal) / (1.0 - optimal)) * 45.0;
      }
    }
  }

  // Penalties
  if (kIndex >= 7)
    rel *= 0.1;
  else if (kIndex >= 6)
    rel *= 0.2;
  else if (kIndex >= 5)
    rel *= 0.4;
  else if (kIndex >= 4)
    rel *= 0.6;
  else if (kIndex >= 3)
    rel *= 0.8;

  double hops = std::ceil(distKm / 3500.0);
  if (hops > 1.0) {
    rel *= std::pow(0.92, hops - 1.0);
  }

  if (std::abs(midLat) > 60.0) {
    rel *= 0.7;
    if (kIndex >= 3)
      rel *= 0.7;
  }

  // Frequency/Solar Flux specific penalties
  if (freqMhz >= 21.0 && sfi < 100.0)
    rel *= std::sqrt(sfi / 100.0);
  if (freqMhz >= 28.0 && sfi < 120.0)
    rel *= std::sqrt(sfi / 120.0);
  if (freqMhz >= 50.0 && sfi < 150.0)
    rel *= std::pow(sfi / 150.0, 1.5);

  // Nighttime low band enhancement / high band penalty
  double localHour = std::fmod(hour + midLon / 15.0 + 24.0, 24.0);
  bool isNight = localHour < 6.0 || localHour > 18.0;

  if (freqMhz <= 7.0 && isNight)
    rel *= 1.1;
  if (freqMhz <= 3.5 && !isNight)
    rel *= 0.7;

  return std::min(99.0, std::max(0.0, rel));
}

// Haversine helper
static double haversineKm(double lat1, double lon1, double lat2, double lon2) {
  double R = 6371.0;
  double dLat = (lat2 - lat1) * M_PI / 180.0;
  double dLon = (lon2 - lon1) * M_PI / 180.0;
  double a = std::sin(dLat / 2) * std::sin(dLat / 2) +
             std::cos(lat1 * M_PI / 180.0) * std::cos(lat2 * M_PI / 180.0) *
                 std::sin(dLon / 2) * std::sin(dLon / 2);
  double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));
  return R * c;
}

std::vector<float>
PropEngine::generateGrid(const PropPathParams &params, const SolarData &sw,
                         const class IonosondeProvider *ionoProvider,
                         int outputType) {
  // outputType: 0=MUF, 1=Reliability

  std::vector<float> grid;
  grid.resize(MAP_W * MAP_H);

  double sfi = (sw.sfi > 0) ? (double)sw.sfi : 70.0;
  double ssn = (sw.sunspot_number > 0) ? (double)sw.sunspot_number : 50.0;
  double kIndex = (double)sw.k_index;

  // If SFI is super low but SSN is decent (or vice versa), sanity check?
  // HamClock usually trusts the provder.

  // Calculate signal margin once
  double marginDb = calculateSignalMargin(params.mode, params.watts);

  // Current UTC hour for "now"
  // In a real forecasted map we might iterate hours, but here we do "now"
  // We can get UTC from system time or pass it in. Let's assume params
  // implicitly means "now". Or we should add UTC to params. For now, use system
  // UTC.
  std::time_t t = std::time(nullptr);
  std::tm *ptm = std::gmtime(&t);
  double utcHour = ptm->tm_hour + ptm->tm_min / 60.0;

  // Pre-calculate ionosonde interpolation for the whole globe?
  // No, do it per point (lazy) or maybe coarse grid?
  // Per-point (660x330 = 217k) calls to interpolate() might be slow if
  // interpolate loops 100 stations. IonosondeProvider::interpolate checks
  // distances. Optimization: Cache standard ionosphere or use a coarse grid and
  // lerp. For Phase 2, let's try direct per-pixel and see performance. If slow,
  // we optimize.

  // To speed up, we can skip ionosonde for points > 3000km from any station?
  // Or just rely on the fallback in calculateMUF.

  for (int y = 0; y < MAP_H; ++y) {
    double lat = 90.0 - (y * 180.0 / MAP_H);
    for (int x = 0; x < MAP_W; ++x) {
      double lon = (x * 360.0 / MAP_W) - 180.0;

      // Should we skip our own location? (dist=0)
      double dist = haversineKm(params.txLat, params.txLon, lat, lon);
      if (dist < 10.0) {
        // At TX location, 100% reliable or max MUF?
        grid[y * MAP_W + x] = (outputType == 0) ? 50.0f : 100.0f;
        continue;
      }

      // Midpoint
      // Simplified midpoint on sphere
      // (Strictly we should do great circle mid, but avg is okay for short
      // paths, for long paths it might be wrong. Let's do simple avg for
      // speed). Proper midpoint requires vector math or careful handling of
      // wrap.

      double phi1 = params.txLat * M_PI / 180.0;
      double lam1 = params.txLon * M_PI / 180.0;
      double phi2 = lat * M_PI / 180.0;
      double lam2 = lon * M_PI / 180.0;

      double Bx = std::cos(phi2) * std::cos(lam2 - lam1);
      double By = std::cos(phi2) * std::sin(lam2 - lam1);
      double midPhi = std::atan2(
          std::sin(phi1) + std::sin(phi2),
          std::sqrt((std::cos(phi1) + Bx) * (std::cos(phi1) + Bx) + By * By));
      double midLam = lam1 + std::atan2(By, std::cos(phi1) + Bx);

      double midLatDeg = midPhi * 180.0 / M_PI;
      double midLonDeg = midLam * 180.0 / M_PI;

      // Interpolate iono
      // TODO: optimize access to provider?
      // Actually, we can create a temporary IonosondeData struct with 0s if
      // provider is null
      InterpolatedIonosonde iono;
      if (ionoProvider) {
        // This is the bottleneck! 217k calls.
        // We MUST optimize this in Phase 3 or use a 10-degree step.
        // For now, let's assume we do it every 5th pixel and lerp?
        // Or just do physics math.
        // Let's rely on the fact that interpolate() inside provider is fast
        // enough for now (it sorts 50 stations). 50 * 217k = 10 million ops.
        // Doable in < 1 sec on modern CPU.
        iono = ionoProvider->interpolate(midLatDeg, midLonDeg);
      }

      if (outputType == 0) {
        // MUF
        float val = (float)calculateMUF(dist, midLatDeg, midLonDeg, utcHour,
                                        sfi, ssn, iono);
        grid[y * MAP_W + x] = val;
      } else {
        // Reliability
        float val = (float)calculateReliability(
            params.mhz, dist, midLatDeg, midLonDeg, utcHour, sfi, ssn, kIndex,
            iono, utcHour, marginDb);
        grid[y * MAP_W + x] = val;
      }
    }
  }

  return grid;
}
