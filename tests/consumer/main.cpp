#include <ecm/lab.hpp>

#include <cmath>

int main() {
    const ecm_cont::RouteInput leg{{1000, 20, 20, 20, 0, 0}, 800};
    const auto route = ecm_cont::cycle_route_energy(leg, {}, {0.9, 0.6, 2500}, {4.8, 0.22, 0.92});
    int rows = 0;
    ecm::lab::run_experiment(ecm::lab::default_route(), {}, {2, 42},
                             [&](const ecm::lab::TrialResult&) { ++rows; });
    return rows == 6 && route.seconds == 50.0 && route.solar_wh > 0.0 && std::isfinite(route.net_wh()) ? 0 : 1;
}
