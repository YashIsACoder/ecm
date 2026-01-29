#include "ecm_cont/integrator.hpp"

namespace ecm_cont {

// see wikipedia 
State RK4::step(const RHS& f, double t, const State& x, double dt)
{
    State k1 = f(t, x);
    State k2 = f(t + 0.5*dt, x + 0.5*dt * k1);
    State k3 = f(t + 0.5*dt, x + 0.5*dt * k2);
    State k4 = f(t + dt,     x + dt * k3);

    return x + (dt/6.0) * (k1 + 2*k2 + 2*k3 + k4);
}

}
