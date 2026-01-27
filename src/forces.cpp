#include <ecm/forces.hpp>
#include <cmath>

namespace ecm {

Forces::Forces(const VehicleParams& p)
	: p_ { p }
	{}

double Forces::rolling() const noexcept {
	return p_.m * p_.g * p_.Crr;
}

double Forces::aero(double v) const noexcept {
	return 0.5 * p_.rho * p_.Cd * p_.A * v * v;
}

double Forces::grade(double slope) const noexcept {
	if (slope == 0.0) return 0.0;
	double theta = std::atan(slope);
	return p_.m * p_.g * std::sin(theta);
}

}
