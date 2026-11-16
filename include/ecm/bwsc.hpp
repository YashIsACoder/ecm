#pragma once

namespace ecm::lab {

struct BwscStageTiming {
    double scheduled_finish_hours;
    double late_hours;
    double penalty_hours;
    double next_start_hours;
};

// BWSC 2025 regulation 3.21.4, using continuous hours from day one's 08:00.
// The given stop is the end of driving for a planned day.
[[nodiscard]] BwscStageTiming bwsc_2025_stage_timing(int day, double stop_driving_hours);

}  // namespace ecm::lab
