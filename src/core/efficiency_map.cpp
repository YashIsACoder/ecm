#include "ecm/efficiency_map.hpp"

#include "detail/energy_inputs.hpp"

#include <algorithm>
#include <functional>
#include <stdexcept>
#include <utility>

namespace ecm {
namespace {

bool valid_axis(const std::vector<double>& axis) {
    return axis.size() >= 2 && std::ranges::all_of(axis, detail::nonnegative) &&
        std::ranges::adjacent_find(axis, std::greater_equal{}) == axis.end();
}

struct Bracket {
    std::size_t lower;
    double fraction;
};

Bracket bracket(const std::vector<double>& axis, double value) {
    value = std::clamp(value, axis.front(), axis.back());
    const auto upper = std::upper_bound(axis.begin(), axis.end() - 1, value);
    const auto lower = static_cast<std::size_t>(upper - axis.begin() - 1);
    const double fraction = (value - axis[lower]) / (axis[lower + 1] - axis[lower]);
    return {lower, fraction};
}

}  // namespace

EfficiencyMap::EfficiencyMap(std::vector<double> wheel_rad_s, std::vector<double> wheel_nm,
                             std::vector<double> efficiencies)
    : speed_(std::move(wheel_rad_s)), torque_(std::move(wheel_nm)), values_(std::move(efficiencies)) {
    if (!valid_axis(speed_) || !valid_axis(torque_) ||
        values_.size() / torque_.size() != speed_.size() || values_.size() % torque_.size() != 0 ||
        !std::ranges::all_of(values_, [](double eta) { return eta > 0.0 && eta <= 1.0; })) {
        throw std::invalid_argument("efficiency map requires increasing nonnegative axes and a complete (0, 1] grid");
    }
}

double EfficiencyMap::at(double wheel_rad_s, double wheel_nm) const {
    if (speed_.empty() || torque_.empty() || values_.empty()) {
        throw std::logic_error("cannot query a moved-from efficiency map");
    }
    if (!detail::nonnegative(wheel_rad_s) || !detail::nonnegative(wheel_nm)) {
        throw std::invalid_argument("efficiency map coordinates must be finite and nonnegative");
    }
    const auto [row, speed_fraction] = bracket(speed_, wheel_rad_s);
    const auto [column, torque_fraction] = bracket(torque_, wheel_nm);
    const auto lower = row * torque_.size() + column;
    const auto upper = lower + torque_.size();
    const double low = std::lerp(values_[lower], values_[lower + 1], torque_fraction);
    const double high = std::lerp(values_[upper], values_[upper + 1], torque_fraction);
    return std::lerp(low, high, speed_fraction);
}

}  // namespace ecm
