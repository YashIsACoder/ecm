#pragma once

#include "ecm/lab.hpp"

void write_atlas_report(std::span<const ecm::lab::Leg> route, ecm::lab::Experiment experiment,
                        std::uint64_t traced_trial, std::string_view model_name,
                        double gust_sd_mps, std::ostream& output);
