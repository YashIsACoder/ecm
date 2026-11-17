#pragma once

#include "ecm/fleet.hpp"

void write_atlas_report(std::span<const ecm::lab::Leg> route, ecm::lab::Experiment experiment,
                        std::uint64_t traced_trial, std::string_view model_name,
                        double gust_sd_mps, std::optional<ecm::lab::FleetEntry> custom,
                        std::ostream& output);
