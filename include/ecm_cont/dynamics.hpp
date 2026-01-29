#pragma once
#include "ecm/params.hpp"
#include "ecm/efficiency.hpp"
#include "integrator.hpp"

namespace ecm_cont {

class Dynamics {
public:
    Dynamics(const ecm::VehicleParams& p,
             const ecm::EfficiencyModel& eff,
             double Pm,
             double wind,
             double slope,
             double aux);

    State operator()(double t, const State& x) const;

private:
    ecm::VehicleParams p_;
    const ecm::EfficiencyModel& eff_;
    double Pm_;
    double wind_;
    double slope_;
    double aux_;
};

}
