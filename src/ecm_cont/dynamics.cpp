#include "ecm_cont/dynamics.hpp"
#include <cmath>
#include <algorithm>

namespace ecm_cont {

Dynamics::Dynamics(const ecm::VehicleParams& p,
		   const ecm::EfficiencyModel& eff,
		   double Pm, 
		   double wind,
		   double slope,
		   double aux)
	: p_{p}, eff_{eff}, Pm_{Pm}, 
	  wind_{wind}, slope_{slope}, aux_{aux}
	{}

State Dynamics::operator()(double, const State& x) const
{
    double v = std::max(x[0], 1e-6);

    double F_aero = 0.5 * p_.rho * p_.Cd * p_.A * (v + wind_) * (v + wind_);
    double F_roll = p_.m * p_.g * p_.Crr;
    double F_grade = p_.m * p_.g * std::sin(slope_);

    double F_res = F_aero + F_roll + F_grade;
    double F_drive = Pm_ / v;

    double dv = (F_drive - F_res) / p_.m;

    double omega = v / p_.rw;
    double torque = F_drive * p_.rw;

    double eta = std::max(eff_.eff(torque, omega), p_.min_eff);
    double dE = -(Pm_ / eta) - aux_;

    return { dv, dE };
}

}
