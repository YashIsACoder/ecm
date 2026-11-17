#pragma once

#include "ecm/lab.hpp"

void write_atlas_report(std::span<const ecm::lab::Leg> route, ecm::lab::Experiment experiment,
                        std::uint64_t traced_trial, const ecm::lab::RouteEnergyModel& model,
                        std::ostream& output);
