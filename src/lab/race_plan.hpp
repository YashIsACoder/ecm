#pragma once

#include "ecm/lab.hpp"

namespace ecm::lab::detail {

// One arrival energy requirement per leg. Empty for the tier strategy.
std::vector<double> prepare_race(std::span<const Leg> route, const Config& config, bool plan_reserve = true);
Result simulate_prepared(std::span<const Leg> route, std::span<const Weather> weather,
                         const Config& config, Policy policy, std::span<const double> arrival_targets,
                         const LegSink& trace = {});

}  // namespace ecm::lab::detail
