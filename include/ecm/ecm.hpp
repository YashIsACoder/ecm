#pragma once
#include "params.hpp"
#include "forces.hpp"
#include "efficiency.hpp"
#include <memory>

namespace ecm {

class ECM {
public:
	ECM(const VehicleParams& params,
	    std::shared_ptr<EfficiencyModel> eff);

	double stint(double vi, double vt, double t,
		     double wind=0.0, double slope=0.0, double S=670.0) const noexcept;
	
private:
	VehicleParams p_;
	Forces forces_;
	std::shared_ptr<EfficiencyModel> eff_;
};

}
