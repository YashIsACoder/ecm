#pragma once

#include <vector>

namespace ecm {

// Rectangular wheel-equivalent motor map. Values are flattened speed-major,
// with one row per angular speed and one column per torque.
class EfficiencyMap {
public:
    EfficiencyMap(std::vector<double> wheel_rad_s, std::vector<double> wheel_nm,
                  std::vector<double> efficiencies);

    // Bilinear interpolation. Nonnegative coordinates outside the map clamp to
    // its edges. Invalid axes, efficiencies or query coordinates throw.
    [[nodiscard]] double at(double wheel_rad_s, double wheel_nm) const;

private:
    std::vector<double> speed_;
    std::vector<double> torque_;
    std::vector<double> values_;
};

}  // namespace ecm
