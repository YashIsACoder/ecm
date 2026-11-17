#pragma once

#include "ecm/lab.hpp"

namespace ecm::lab {

struct FleetEntry {
    Config config;
    Policy policy = Policy::reserve_first;
};

struct FleetResult {
    std::uint64_t trial;
    std::size_t entry;  // Zero-based index into the caller's fleet.
    Result result;
};

using FleetSink = std::function<void(const FleetResult&)>;

struct FleetTrace {
    std::uint64_t trial = 0;
    std::function<void(std::size_t entry, const LegObservation&)> sink;
};

// Streams results in trial and entry order. All entrants see the same draw.
// Each entrant's route energy model, rules, and policy can be set independently.
void run_fleet(std::span<const Leg> route, std::span<const FleetEntry> fleet,
               Experiment experiment, const FleetSink& sink, const FleetTrace& trace = {});

struct CruiserProfile {
    std::string_view id;
    std::string_view team;
    std::string_view car;
    std::string_view region;
    FleetEntry entry;
};

// Five 2025 entrants. Names are historical. Vehicle inputs are illustrative
// scenarios under the same modeled Cruiser energy and timing constraints.
[[nodiscard]] std::vector<CruiserProfile> bwsc_2025_cruiser_profiles();

// The built-in corridor with leg distances reconciled to published 2025 control distances.
// Minor waypoint spacing, grades and weather inputs remain scenario estimates.
[[nodiscard]] std::vector<Leg> bwsc_2025_cruiser_route();

}  // namespace ecm::lab
