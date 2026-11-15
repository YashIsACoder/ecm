#include "ecm_cont/integrator.hpp"

namespace ecm_cont {

// Euler
State Euler::step(const RHS& f, double t, const State& x, double dt) {
	return x + dt * f(t, x);
}

// Semi Implicit Euler
State SemiImplicitEuler::step(const RHS& f, double t, const State& x, double dt) {
	State dx = f(t,x);

	State x_next = x;
	x_next[0] += dt*dx[0];
	x_next[1] += dt*dx[1];
	return x_next;
}

// RK4
State RK4::step(const RHS& f, double t, const State& x, double dt)
{
    State k1 = f(t, x);
    State k2 = f(t + 0.5*dt, x + 0.5*dt * k1);
    State k3 = f(t + 0.5*dt, x + 0.5*dt * k2);
    State k4 = f(t + dt,     x + dt * k3);

    return x + (dt/6.0) * (k1 + 2*k2 + 2*k3 + k4);
}

}
