#include "ecm_cont/route_energy.hpp"

#include <cmath>
#include <stdexcept>

namespace {

void near(double actual, double expected, const char* message) {
    if (!(std::abs(actual - expected) < 1e-9)) {
        throw std::runtime_error(message);
    }
}

}  // namespace

int main() {
    const ecm::VehicleParams vehicle;
    const ecm::Drivetrain drivetrain{0.9, 0.5, 900};
    const ecm::SolarArray array{4, 0.20, 0.9};
    const ecm_cont::RouteInput cruise{{1000, 20, 20, 20, 0.01, 1}, 900};
    const auto segment = ecm_cont::segment_route_energy(cruise, vehicle, drivetrain, array);
    const auto cycle = ecm_cont::cycle_route_energy(cruise, vehicle, drivetrain, array);
    near(cycle.seconds, segment.seconds, "same cruising duration");
    near(cycle.net_wh(), segment.net_wh(), "same cruising demand");
    if (!cycle.components || !segment.components) {
        throw std::runtime_error("built-in route estimators must report their native energy components");
    }
    near(cycle.components->rolling_wh, segment.components->rolling_wh, "same rolling work");
    near(cycle.components->drag_wh, segment.components->drag_wh, "same aerodynamic work");
    near(cycle.components->climb_wh, segment.components->climb_wh, "same gravitational work");

    const ecm::EfficiencyMap map({0, 100}, {0, 1000}, {0.9, 0.9, 0.9, 0.9});
    const auto mapped = ecm_cont::mapped_route_energy(cruise, vehicle, drivetrain, array, map);
    near(mapped.net_wh(), cycle.net_wh(), "constant map agrees with fixed drive efficiency");

    const ecm_cont::RouteInput short_leg{{100, 20, 0, 0, 0, 0}, 0};
    const auto triangular = ecm_cont::route_speed_profile(short_leg, {1, 1});
    if (triangular.size() != 2) {
        throw std::runtime_error("short leg should require a triangular speed profile");
    }
    near(triangular.front().final_speed_mps, 10, "short leg peak is adjusted");
    near(ecm_cont::drive_cycle(vehicle, triangular, drivetrain).distance_m, 100, "triangle covers exact distance");

    const ecm_cont::RouteInput valley_leg{{100, 5, 20, 20, 0, 0}, 0};
    const auto valley = ecm_cont::route_speed_profile(valley_leg, {1, 1});
    near(valley.front().final_speed_mps, std::sqrt(300.0), "short valley reduces braking");
    near(ecm_cont::drive_cycle(vehicle, valley, drivetrain).distance_m, 100, "valley covers exact distance");

    const ecm_cont::RouteInput long_leg{{1000, 20, 0, 0, 0, 0}, 0};
    const auto trapezoid = ecm_cont::route_speed_profile(long_leg, {1, 2});
    if (trapezoid.size() != 3) {
        throw std::runtime_error("long leg should include all three phases");
    }
    near(ecm_cont::drive_cycle(vehicle, trapezoid, drivetrain).distance_m, 1000, "ramps and cruise cover exact distance");

    try {
        (void)ecm_cont::route_speed_profile({{100, 20, 0, 20, 0, 0}, 0}, {1, 1});
        throw std::runtime_error("impossible boundary speeds accepted");
    } catch (const std::invalid_argument&) {
        // Expected.
    }
}
