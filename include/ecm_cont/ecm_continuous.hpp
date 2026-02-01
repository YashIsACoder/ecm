#pragma once
#include "ecm/params.hpp"
#include "ecm/efficiency.hpp"
#include "ecm_cont/ecm_continuous.hpp"
#include "ecm_cont/integrator_dispatch.hpp"

namespace ecm_cont {

class ECMContinuous {
public:
    ECMContinuous(const ecm::VehicleParams& p,
                  const ecm::EfficiencyModel& eff,
		  IntegratorType integrator = IntegratorType::RK4);

    double stint(double vi,
                 double vt,
                 double T,
                 double wind=0.0,
                 double slope=0.0,
                 double aux =30.0) const;

private:
    double solve_power(double vi, double vt, double T,
                       double wind, double slope, double aux) const;

    ecm::VehicleParams p_;
    const ecm::EfficiencyModel& eff_;
    IntegratorType integrator_;
};

}
