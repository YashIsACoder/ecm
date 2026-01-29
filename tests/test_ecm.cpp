#include <cassert>
#include <iostream>

#include "ecm/ecm.hpp"
#include "ecm/efficiency.hpp"

int main()
{
    ecm::VehicleParams params{
        .rho = 1.225,
        .Cd  = 0.1,
        .A   = 1.864,
        .m   = 932.8,
        .g   = 9.81,
        .Crr = 0.0105,
        .rw  = 0.2715,
	.X = 30, // added this 
    };

    auto eff = std::make_shared<ecm::StepEfficiency>();
    ecm::ECM model(params, eff);

    double energy = model.stint(
        10.0,  // vi
        12.0,  // vt
	1.0,    // t
	0.0,	// wind
        0.0,	// slope
        570.0 // solar input avg
    );

    std::cout << "Energy: " << energy << '\n';

    assert(energy > 0.0);
    assert(energy < 1e6);

    std::cout << "[C++] ECM stint test passed\n";
}

