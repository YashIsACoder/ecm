#include "ecm/efficiency.hpp"
#include <cmath>
#include <algorithm>

namespace ecm {

StepEfficiency::StepEfficiency() {
	thresholds_ = {
	1.0/175.0, 15.0/1250.0, 23.0/1250.0, 31.0/1250.0,
	31.0/1000.0, 29.0/750.0, 33.0/750.0, 26.0/500.0,
	30.0/500.0, 33.0/500.0, 1.0
	};
}

double StepEfficiency::eff(double torque, double v) const noexcept {
	double rpm = (v == 0.0) ? 0.0 : (v * 30.0 / M_PI);
	double ratio = (rpm == 0.0) ? 0.0 : torque/rpm;

	for (size_t i{}; i < thresholds_.size(); ++i)
		if (ratio < thresholds_[i])
			return std::max(0.99 - static_cast<double>(i) / 100.0, 0.0);

	return 0.99;
}

}
