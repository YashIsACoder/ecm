#pragma once
#include <array>
#include <functional>

namespace ecm_cont {

template <typename T>
using State = std::array<T, 2>; // {v, E}

inline State operator+(const State& a, const State&b) {
	return { a[0] + b[0], a[1] + b[1] };
}

inline State operator*(double s, const State& a) {
	return { s*a[0], s*a[1] };
}

inline State operator*(const State& a, double s) {
	return s*a;
}

// RHS: dx/dt = f(t, x)
using RHS = std::function<State(double, const State&)>;
	
struct Euler {
	static State step(const RHS& f, double t, const State& x, double dt);
};

struct SemiImplicitEuler {
	static State step(const RHS& f, double t, const State& x, double dt);
};

struct RK4 {
	static State step(const RHS& f, double t, const State& x, double dt);
};

}
