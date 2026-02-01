#pragma once
#include "integrator.hpp"

namespace ecm_cont {

enum class IntegratorType {
    Euler,
    SemiImplicitEuler,
    RK4
};

inline State step_integrator(
    IntegratorType type,
    const RHS& f,
    double t,
    const State& x,
    double dt)
{
    switch (type) {
    case IntegratorType::Euler:
        return Euler::step(f, t, x, dt);
    case IntegratorType::SemiImplicitEuler:
        return SemiImplicitEuler::step(f, t, x, dt);
    case IntegratorType::RK4:
    default:
        return RK4::step(f, t, x, dt);
    }
}

}

