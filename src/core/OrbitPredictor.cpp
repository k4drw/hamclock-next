#include "OrbitPredictor.h"

#include <cmath>
#include <cstdio>

OrbitPredictor::OrbitPredictor() = default;

OrbitPredictor::~OrbitPredictor()
{
    if (elements_) predict_destroy_orbital_elements(elements_);
    if (observer_) predict_destroy_observer(observer_);
}

void OrbitPredictor::setObserver(double latDeg, double lonDeg, double altMeters)
{
    if (observer_) predict_destroy_observer(observer_);
    observer_ = predict_create_observer("QTH",
        latDeg * kDeg2Rad, lonDeg * kDeg2Rad, altMeters);
    if (!observer_) {
        std::fprintf(stderr, "OrbitPredictor: failed to create observer\n");
    }
}

bool OrbitPredictor::loadTLE(const SatelliteTLE& tle)
{
    if (elements_) {
        predict_destroy_orbital_elements(elements_);
        elements_ = nullptr;
    }
    satName_.clear();

    elements_ = predict_parse_tle(tle.line1.c_str(), tle.line2.c_str());
    if (!elements_) {
        std::fprintf(stderr, "OrbitPredictor: failed to parse TLE for %s\n",
                     tle.name.c_str());
        return false;
    }
    satName_ = tle.name;
    return true;
}

bool OrbitPredictor::isReady() const
{
    return observer_ != nullptr && elements_ != nullptr;
}

SatObservation OrbitPredictor::observe() const
{
    return observeAt(std::time(nullptr));
}

SatObservation OrbitPredictor::observeAt(std::time_t utc) const
{
    SatObservation result;
    if (!isReady()) return result;

    predict_julian_date_t jd = predict_to_julian(utc);

    struct predict_position pos{};
    predict_orbit(elements_, &pos, jd);

    struct predict_observation obs{};
    predict_observe_orbit(observer_, &pos, &obs);

    result.azimuth   = obs.azimuth * kRad2Deg;
    result.elevation = obs.elevation * kRad2Deg;
    result.range     = obs.range;
    result.rangeRate = obs.range_rate;
    result.visible   = obs.visible;

    // Normalize azimuth to [0, 360)
    result.azimuth = std::fmod(result.azimuth + 360.0, 360.0);

    return result;
}

SubSatPoint OrbitPredictor::subSatPoint() const
{
    return subSatPointAt(std::time(nullptr));
}

SubSatPoint OrbitPredictor::subSatPointAt(std::time_t utc) const
{
    SubSatPoint result;
    if (!elements_) return result;

    predict_julian_date_t jd = predict_to_julian(utc);

    struct predict_position pos{};
    predict_orbit(elements_, &pos, jd);

    result.lat       = pos.latitude * kRad2Deg;
    result.lon       = pos.longitude * kRad2Deg;
    result.altitude  = pos.altitude;
    result.footprint = pos.footprint;

    // Normalize longitude to [-180, 180]
    while (result.lon > 180.0)  result.lon -= 360.0;
    while (result.lon < -180.0) result.lon += 360.0;

    return result;
}

SatPass OrbitPredictor::nextPass() const
{
    return nextPassAfter(std::time(nullptr));
}

SatPass OrbitPredictor::nextPassAfter(std::time_t utc) const
{
    SatPass result;
    if (!isReady()) return result;

    predict_julian_date_t jd = predict_to_julian(utc);

    // Find AOS
    struct predict_observation aos = predict_next_aos(observer_, elements_, jd);
    result.aosTime = predict_from_julian(aos.time);
    result.aosAz   = std::fmod(aos.azimuth * kRad2Deg + 360.0, 360.0);

    // Find LOS (starting from AOS)
    struct predict_observation los = predict_next_los(observer_, elements_, aos.time);
    result.losTime = predict_from_julian(los.time);
    result.losAz   = std::fmod(los.azimuth * kRad2Deg + 360.0, 360.0);

    // Find max elevation during pass
    struct predict_observation maxEl = predict_at_max_elevation(observer_, elements_, aos.time);
    result.maxEl = maxEl.elevation * kRad2Deg;

    return result;
}

std::vector<GroundTrackPoint> OrbitPredictor::groundTrack(std::time_t startUtc,
                                                          int minutes,
                                                          int stepSec) const
{
    std::vector<GroundTrackPoint> track;
    if (!elements_) return track;

    int totalSec = minutes * 60;
    int numPoints = totalSec / stepSec + 1;
    track.reserve(numPoints);

    for (int s = 0; s < totalSec; s += stepSec) {
        std::time_t t = startUtc + s;
        predict_julian_date_t jd = predict_to_julian(t);

        struct predict_position pos{};
        predict_orbit(elements_, &pos, jd);

        double lat = pos.latitude * kRad2Deg;
        double lon = pos.longitude * kRad2Deg;
        while (lon > 180.0)  lon -= 360.0;
        while (lon < -180.0) lon += 360.0;

        track.push_back({lat, lon});
    }

    return track;
}

double OrbitPredictor::tleAgeDays() const
{
    if (!elements_) return -1.0;

    // Convert TLE epoch (2-digit year + day of year) to time_t
    int yr = elements_->epoch_year;
    yr += (yr < 57) ? 2000 : 1900;  // Y2K convention: <57 → 20xx, ≥57 → 19xx

    struct std::tm epoch_tm{};
    epoch_tm.tm_year = yr - 1900;
    epoch_tm.tm_mday = 1;  // Jan 1
    epoch_tm.tm_isdst = 0;
    std::time_t jan1 = timegm(&epoch_tm);

    double epochSec = static_cast<double>(jan1) + (elements_->epoch_day - 1.0) * 86400.0;
    double nowSec = static_cast<double>(std::time(nullptr));
    return (nowSec - epochSec) / 86400.0;
}

double OrbitPredictor::dopplerShift(double downlinkHz) const
{
    if (!isReady()) return 0.0;

    // Get current observation for range_rate
    predict_julian_date_t jd = predict_to_julian(std::time(nullptr));

    struct predict_position pos{};
    predict_orbit(elements_, &pos, jd);

    struct predict_observation obs{};
    predict_observe_orbit(observer_, &pos, &obs);

    return predict_doppler_shift(&obs, downlinkHz);
}
