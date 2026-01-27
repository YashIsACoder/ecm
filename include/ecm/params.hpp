#pragma once

namespace ecm {
struct VehicleParams {
	double rho = 1.225; // air density (kg/m^3)
	double Cd = 0.1; // drag coefficient
	double A = 1.864; // frontal area (m^2)
	double m = 932.8; // mass (kg)
	double g = 9.81; // gravity (m/s^2)
	double Crr = 0.0105; // rolling resistance
	double rw = 0.2715; // wheel radius (m)
	double X = 30.0; // auxiliary power (W)
	double min_eff = 1e-3;
};
}
