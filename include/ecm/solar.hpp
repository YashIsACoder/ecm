#pragma once

#include <chrono>

namespace ecm::lab {

struct GeographicPoint {
    double latitude_deg;   // North positive, [-90, 90].
    double longitude_deg;  // East positive, [-180, 180].
};

struct SunDirection {
    double x;  // Greenwich meridian at the equator.
    double y;  // 90 degrees east at the equator.
    double z;  // North pole.
};

struct SolarPosition {
    double east;
    double north;
    double up;
    double azimuth_deg;    // Clockwise from true north, [0, 360).
    double elevation_deg;  // Geometric elevation, before atmospheric refraction.
};

// NOAA fractional-year approximation. Input is a UTC instant.
[[nodiscard]] SunDirection sun_direction(std::chrono::sys_seconds utc);
[[nodiscard]] SolarPosition solar_position(std::chrono::sys_seconds utc, GeographicPoint point);

// Project the direct beam onto an array. Tilt is from horizontal and the
// array azimuth is the direction the panel faces. Returns zero below the horizon.
[[nodiscard]] double direct_beam_factor(const SolarPosition& sun, double tilt_deg, double array_azimuth_deg);

}  // namespace ecm::lab
