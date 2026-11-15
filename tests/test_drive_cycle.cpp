#include "ecm_cont/drive_cycle.hpp"

#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {

void near(double actual, double expected, const char* message) {
    if (!(std::abs(actual - expected) <= 1e-10)) {
        throw std::runtime_error(message);
    }
}

void cruise_matches_segment() {
    const ecm::VehicleParams vehicle{.rho = 1.2, .Cd = 0.14, .A = 1.0, .m = 200.0, .Crr = 0.005, .X = 60.0};
    const ecm::Drivetrain drivetrain{0.90, 0.55, 100.0};
    const ecm::SolarArray array{4.0, 0.20, 0.90};
    const std::array profile{ecm_cont::SpeedInterval{1800.0, 20.0, 20.0, 0.08, 3.0, 1000.0}};
    const auto cycle = ecm_cont::drive_cycle(vehicle, profile, drivetrain, array);
    const auto segment = ecm::drive_energy(vehicle, {36000.0, 20.0, 20.0, 20.0, 0.08, 3.0}, drivetrain);
    near(cycle.distance_m, 36000.0, "constant-speed distance");
    near(cycle.wheel_wh, segment.wheel_wh(), "steady mechanical work agrees with segment");
    near(cycle.traction_wh, segment.traction_wh, "steady traction agrees with segment");
    near(cycle.solar_wh, 360.0, "solar energy at constant irradiance");
    near(cycle.net_dc_wh(), segment.demand_wh() - 360.0, "DC energy balance");
}

void start_stop_and_idle() {
    const ecm::VehicleParams vehicle{.Cd = 0.0, .m = 200.0, .Crr = 0.0, .X = 100.0};
    const ecm::Drivetrain drivetrain{0.8, 0.5, 10000.0};
    const std::array profile{
        ecm_cont::SpeedInterval{10.0, 0.0, 10.0, 0.0, 0.0, 0.0},
        ecm_cont::SpeedInterval{10.0, 10.0, 0.0, 0.0, 0.0, 0.0},
        ecm_cont::SpeedInterval{5.0, 0.0, 0.0, 0.0, 0.0, 0.0},
    };
    const auto cycle = ecm_cont::drive_cycle(vehicle, profile, drivetrain);
    near(cycle.seconds, 25.0, "drive-cycle duration includes idle");
    near(cycle.distance_m, 100.0, "linear speed integrates to trapezoidal distance");
    near(cycle.wheel_wh, 0.0, "kinetic-energy gain and loss cancel");
    near(cycle.traction_wh, (10000.0 / 0.8 - 10000.0 * 0.5) / 3600.0, "drivetrain losses remain");
    near(cycle.auxiliary_wh, 2500.0 / 3600.0, "auxiliaries draw power while stopped");
}

void brackets_both_regeneration_crossings() {
    const ecm::VehicleParams vehicle{.rho = 2.0, .Cd = 1.0, .A = 1.0, .m = 10.0, .g = 10.0, .Crr = 0.0, .X = 0.0};
    const ecm::Drivetrain drivetrain{0.8, 1.0, 96.0};
    const double grade = -0.62 / std::sqrt(1.0 - 0.62 * 0.62);
    const std::array profile{ecm_cont::SpeedInterval{8.0, 0.0, 8.0, grade, 0.0, 0.0}};
    const auto cycle = ecm_cont::drive_cycle(vehicle, profile, drivetrain);
    // P(v) = v^3 - 52v. It crosses -96 W at 2 and 6 m/s and crosses zero near 7.21 m/s.
    near(cycle.wheel_wh, -640.0 / 3600.0, "signed cubic wheel work");
    near(cycle.traction_wh, -503.0 / 3600.0, "within-interval regen cap and later propulsion loss");
}

void wind_changes_drag_direction() {
    const ecm::VehicleParams vehicle{.rho = 2.0, .Cd = 1.0, .A = 1.0, .m = 10.0, .Crr = 0.0, .X = 0.0};
    const std::array profile{ecm_cont::SpeedInterval{10.0, 0.0, 10.0, 0.0, -5.0, 0.0}};
    const auto cycle = ecm_cont::drive_cycle(vehicle, profile, {1.0, 1.0, 1000.0});
    near(cycle.wheel_wh, (500.0 + 312.5) / 3600.0, "signed drag changes at zero relative airspeed");
    near(cycle.traction_wh, cycle.wheel_wh, "lossless, uncapped drivetrain");
}

void rejects_invalid_profiles() {
    std::array intervals{
        ecm_cont::SpeedInterval{10.0, 0.0, 10.0, 0.0, 0.0, 0.0},
        ecm_cont::SpeedInterval{5.0, 9.0, 0.0, 0.0, 0.0, 0.0},
    };
    try {
        (void)ecm_cont::drive_cycle({}, intervals, {1.0, 0.0, 0.0});
        throw std::runtime_error("discontinuous boundary accepted");
    } catch (const std::invalid_argument&) {
        // Expected.
    }
    intervals[0].final_speed_mps = std::numeric_limits<double>::infinity();
    try {
        (void)ecm_cont::drive_cycle({}, intervals, {1.0, 0.0, 0.0});
        throw std::runtime_error("non-finite speed accepted");
    } catch (const std::invalid_argument&) {
        // Expected.
    }
}

}  // namespace

int main() {
    cruise_matches_segment();
    start_stop_and_idle();
    brackets_both_regeneration_crossings();
    wind_changes_drag_direction();
    rejects_invalid_profiles();
}
