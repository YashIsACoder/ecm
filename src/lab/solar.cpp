#include "ecm/solar.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace ecm::lab {

namespace {

constexpr double radians = std::numbers::pi / 180.0;

}  // namespace

SunDirection sun_direction(std::chrono::sys_seconds utc) {
    using namespace std::chrono;
    const auto date = floor<days>(utc);
    const year year_at_utc = year_month_day{date}.year();
    const double day_index = (date - sys_days{year_at_utc / January / 1}).count();
    const double utc_hours = duration<double, std::ratio<3600>>(utc - date).count();
    const double year_days = year_at_utc.is_leap() ? 366.0 : 365.0;
    const double angle = 2.0 * std::numbers::pi / year_days * (day_index + (utc_hours - 12.0) / 24.0);

    const double equation_minutes = 229.18 * (0.000075 + 0.001868 * std::cos(angle)
        - 0.032077 * std::sin(angle) - 0.014615 * std::cos(2.0 * angle) - 0.040849 * std::sin(2.0 * angle));
    const double declination = 0.006918 - 0.399912 * std::cos(angle) + 0.070257 * std::sin(angle)
        - 0.006758 * std::cos(2.0 * angle) + 0.000907 * std::sin(2.0 * angle)
        - 0.002697 * std::cos(3.0 * angle) + 0.00148 * std::sin(3.0 * angle);
    const double longitude = (180.0 - (utc_hours * 60.0 + equation_minutes) / 4.0) * radians;
    return {std::cos(declination) * std::cos(longitude), std::cos(declination) * std::sin(longitude),
            std::sin(declination)};
}

SolarPosition solar_position(std::chrono::sys_seconds utc, GeographicPoint point) {
    if (!(std::abs(point.latitude_deg) <= 90.0 && std::abs(point.longitude_deg) <= 180.0)) {
        throw std::invalid_argument("latitude or longitude out of range");
    }
    const SunDirection sun = sun_direction(utc);
    const double lat = point.latitude_deg * radians;
    const double lon = point.longitude_deg * radians;
    const double along_meridian = sun.x * std::cos(lon) + sun.y * std::sin(lon);
    const double east = sun.y * std::cos(lon) - sun.x * std::sin(lon);
    const double north = sun.z * std::cos(lat) - along_meridian * std::sin(lat);
    const double up = sun.z * std::sin(lat) + along_meridian * std::cos(lat);
    const double azimuth = std::fmod(std::atan2(east, north) / radians + 360.0, 360.0);
    return {east, north, up, azimuth, std::asin(std::clamp(up, -1.0, 1.0)) / radians};
}

double direct_beam_factor(const SolarPosition& sun, double tilt_deg, double array_azimuth_deg) {
    if (!(tilt_deg >= 0.0 && tilt_deg <= 180.0 && std::isfinite(array_azimuth_deg))) {
        throw std::invalid_argument("invalid array orientation");
    }
    if (sun.up <= 0.0) return 0.0;
    const double tilt = tilt_deg * radians;
    const double azimuth = std::remainder(array_azimuth_deg, 360.0) * radians;
    const double dot = sun.up * std::cos(tilt)
        + std::sin(tilt) * (sun.east * std::sin(azimuth) + sun.north * std::cos(azimuth));
    return std::clamp(dot, 0.0, 1.0);
}

}  // namespace ecm::lab
