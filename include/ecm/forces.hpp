#pragma once
#include "params.hpp"

namespace ecm {

class Forces {
public:
	explicit Forces(const VehicleParams& p);

	double rolling() const noexcept;
	double aero(double v) const noexcept;
	double grade(double slope) const noexcept;

private:
	VehicleParams p_;
};

}
