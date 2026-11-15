#include "ecm/ecm.hpp"
#include "ecm/segment.hpp"

#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>

namespace {

class FixedEfficiency final : public ecm::EfficiencyModel {
public:
    double eff(double, double) const noexcept override { return 0.90; }
};

void near(double actual, double expected, const char* message) {
    if (!(std::abs(actual - expected) < 1e-8)) {
        throw std::runtime_error(message);
    }
}

}  // namespace

int main() {
    const ecm::VehicleParams car{
        .rho = 1.2, .Cd = 0.14, .A = 1.0, .m = 200.0, .g = 9.80665, .Crr = 0.005, .X = 60.0,
    };
    const ecm::Drivetrain drivetrain{0.90, 0.55, 100.0};
    const ecm::RoadSegment flat{36000.0, 20.0, 20.0, 20.0, 0.0, 0.0};
    const auto steady = ecm::drive_energy(car, flat, drivetrain);
    near(steady.seconds, 1800.0, "travel time");
    near(steady.drag_wh, 336.0, "aero work");
    near(steady.rolling_wh, 98.0665, "rolling work");
    near(steady.auxiliary_wh, 30.0, "auxiliary draw");

    const ecm::ECM legacy(car, std::make_shared<FixedEfficiency>());
    near(steady.demand_wh(), legacy.stint(20, 20, 1800, 0, 0, 0) / 3600.0,
         "constant-efficiency range model agrees with legacy flat cruise");

    auto varied = flat;
    varied.grade = 0.08;
    near(ecm::drive_energy(car, varied, drivetrain).climb_wh,
         car.m * car.g * 0.08 / std::hypot(1.0, 0.08) * 10.0, "climb work");
    varied.grade = -0.08;
    near(ecm::drive_energy(car, varied, drivetrain).traction_wh, -50.0, "regeneration cap");

    varied = flat;
    varied.entry_speed_mps = 0.0;
    near(ecm::drive_energy(car, varied, drivetrain).kinetic_wh, 0.5 * car.m * 400.0 / 3600.0,
         "start from rest");
    varied.headwind_mps = -25.0;
    near(ecm::drive_energy(car, varied, drivetrain).drag_wh, -21.0, "wind faster than the car");

    const ecm::SolarArray array{4.0, 0.20, 0.90};
    near(ecm::solar_energy_wh(array, 1000.0, 1800.0), 360.0, "solar generation");
    near(ecm::solar_energy_wh(array, 1000.0, 0.0), 0.0, "zero exposure time");

    try {
        varied.cruise_speed_mps = std::numeric_limits<double>::quiet_NaN();
        (void)ecm::drive_energy(car, varied, drivetrain);
        throw std::runtime_error("non-finite speed accepted");
    } catch (const std::invalid_argument&) {
        // Expected.
    }
    try {
        varied.cruise_speed_mps = 1e200;
        (void)ecm::drive_energy(car, varied, drivetrain);
        throw std::runtime_error("overflowing result accepted");
    } catch (const std::overflow_error&) {
        // Expected.
    }
}
