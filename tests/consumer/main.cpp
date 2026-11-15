#include <ecm_cont/route_energy.hpp>

#include <cmath>

int main() {
    const ecm_cont::RouteInput leg{{1000, 20, 20, 20, 0, 0}, 800};
    const auto route = ecm_cont::cycle_route_energy(leg, {}, {0.9, 0.6, 2500}, {4.8, 0.22, 0.92});
    return route.seconds == 50.0 && route.solar_wh > 0.0 && std::isfinite(route.net_wh()) ? 0 : 1;
}
