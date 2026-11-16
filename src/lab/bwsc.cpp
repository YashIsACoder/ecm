#include "ecm/bwsc.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ecm::lab {

BwscStageTiming bwsc_2025_stage_timing(int day, double stop_driving_hours) {
    if (day < 1 || !std::isfinite(stop_driving_hours) || stop_driving_hours < 0.0) {
        throw std::invalid_argument("stage timing requires a positive day and finite nonnegative elapsed time");
    }

    const double finish = 24.0 * (day - 1) + 9.0;
    const double late = std::max(0.0, stop_driving_hours - finish);
    constexpr double grace_hours = 10.0 / 60.0;
    const double penalty = late + std::max(0.0, late - grace_hours);
    const double next_start = 24.0 * day + penalty;
    if (!std::isfinite(next_start)) throw std::overflow_error("stage timing exceeds numeric range");
    return {finish, late, penalty, next_start};
}

}  // namespace ecm::lab
