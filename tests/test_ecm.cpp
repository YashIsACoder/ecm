#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>

#include "ecm/ecm.hpp"
#include "ecm/efficiency.hpp"
#include "ecm_cont/ecm_continuous.hpp"

int main()
{
    // ---------------- Vehicle parameters ----------------
    ecm::VehicleParams params{
        .rho = 1.225,
        .Cd  = 0.1,
        .A   = 1.864,
        .m   = 932.8,
        .g   = 9.81,
        .Crr = 0.0105,
        .rw  = 0.2715,
        .X   = 30.0,     // auxiliary power
        .min_eff = 1e-3
    };

    auto eff = std::make_shared<ecm::StepEfficiency>();

    // ---------------- Discrete ECM ----------------
    ecm::ECM discrete(params, eff);

    double vi = 10.0;
    double vt = 12.0;
    double T  = 1.0;
    double wind  = 0.0;
    double slope = 0.0;
    double solar = 570.0;

    double E_discrete = discrete.stint(
        vi,
        vt,
        T,
        wind,
        slope,
        solar
    );

    std::cout << "Discrete ECM energy:   " << E_discrete << '\n';

    // ---------------- Continuous ECM ----------------
    ecm_cont::ECMContinuous continuous(params, *eff);

    double E_cont = continuous.stint(
        vi,
        vt,
        T,
        wind,
        slope,
        params.X - solar   // net auxiliary (same interpretation)
    );

    std::cout << "Continuous ECM energy: " << E_cont << '\n';

    // ---------------- Sanity checks ----------------
    assert(E_discrete > 0.0);
    assert(E_cont > 0.0);

    assert(E_discrete < 1e6);
    assert(E_cont < 1e6);

    // ---------------- Consistency check ----------------
    double rel_err =
        std::abs(E_cont - E_discrete) /
        std::max(E_discrete, 1.0);

    std::cout << "Relative error: " << rel_err * 100.0 << " %\n";

    // Expect close agreement (ODE vs discrete)
    assert(rel_err < 0.05); // ≤ 5%

    std::cout << "[C++] Discrete vs Continuous ECM test passed\n";
    return 0;
}

