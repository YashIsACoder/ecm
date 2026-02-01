#include "ecm_cont/ecm_continuous.hpp"
#include "ecm_cont/integrator.hpp"
#include "ecm_cont/dynamics.hpp"

namespace ecm_cont {

ECMContinuous::ECMContinuous(const ecm::VehicleParams& p,
                             const ecm::EfficiencyModel& eff,
			     IntegratorType integrator)
    : p_{p}, eff_{eff}, integrator_{integrator}
	{}

double ECMContinuous::solve_power(double vi, double vt, double T,
                                  double wind, double slope, double aux) const
{
    // simple Newton solve to match v(T)=vt
    double Pm = p_.m * (vt - vi) * vi / T; // our guess

    for (int i = 0; i < 8; ++i) {
        Dynamics dyn(p_, eff_, Pm, wind, slope, aux);
        State x { vi, 0.0 };
        double t = 0.0, dt = T / 200.0;

        while (t < T) {
            x = step_integrator(integrator_, dyn, t, x, dt);
            t += dt;
        }

        double err = x[0] - vt;
        Pm -= 0.5 * err * Pm;
    }

    return Pm;
}

double ECMContinuous::stint(double vi,
                            double vt,
                            double T,
                            double wind,
                            double slope,
                            double aux) const
{
    double Pm = solve_power(vi, vt, T, wind, slope, aux);

    Dynamics dyn(p_, eff_, Pm, wind, slope, aux);
    State x { vi, 0.0 };

    double t = 0.0, dt = 0.001;
    while (t < T) {
        x = step_integrator(integrator_, dyn, t, x, dt);
        t += dt;
    }

    return -x[1];
}

}
