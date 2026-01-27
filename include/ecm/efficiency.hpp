#pragma once
#include <memory>
#include <vector>

namespace ecm {

class EfficiencyModel {
public:
	virtual ~EfficiencyModel() = default; // prevent issues
	virtual double eff(double torque, double v) const noexcept = 0;
};

class StepEfficiency final : public EfficiencyModel {
public:
	StepEfficiency();
	double eff(double torque, double v) const noexcept override;

private:
	std::vector<double> thresholds_;
};

}
