#include <iostream>
#include "ecm/ecm.hpp"
#include <cmath>
#include <algorithm>

namespace ecm {

ECM::ECM(const VehicleParams& params,
	 std::shared_ptr<EfficiencyModel> eff)
	: p_{params}, forces_{params}, eff_{std::move(eff)}
	{}

double ECM::stint(double vi, double vt, double t, 
		  double wind, double slope, double S) const noexcept
{
	if (t <= 0.0) return 0.0;

	double Vi = vi + wind;
	double Vt = vt + wind;
	std::cout << "Vi: " << Vi << '\n';
	std::cout << "Vt: " << Vt << '\n';

	double v2_avg = (Vi*Vi + Vi*Vt + Vt*Vt) / 3.0; // approximation to integral
	double v_avg = std::sqrt(std::max(0.0, v2_avg));
	std::cout << v_avg << '\n';

	double F_inertia = p_.m * (vt - vi) / t;
	double F = F_inertia 
		+ forces_.rolling() 
		+ forces_.aero(v_avg) 
		+ forces_.grade(slope);

	std::cout << F_inertia <<  ' ' << forces_.rolling() << ' ' << forces_.aero(v_avg) << ' ' << forces_.grade(slope) << ' ' << F << '\n';

	double torque = F * p_.rw;
	double v_mid = 0.5 * (vt + vi);

	double eta = eff_->eff(torque, v_mid);
	eta = std::max(eta, p_.min_eff);

	double propulsion = v_mid * (F/eta);
	double power = p_.X - S + propulsion;

	return power*t; // Joules
}

}


