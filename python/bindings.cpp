#include <pybind11/pybind11.h>
#include <memory>
#include "ecm/params.hpp"
#include "ecm/efficiency.hpp"
#include "ecm/ecm.hpp"


namespace py = pybind11;
using namespace ecm;


PYBIND11_MODULE(ecm, m) {
py::class_<VehicleParams>(m, "VehicleParams")
.def(py::init<>())
.def_readwrite("rho", &VehicleParams::rho)
.def_readwrite("Cd", &VehicleParams::Cd)
.def_readwrite("A", &VehicleParams::A)
.def_readwrite("m", &VehicleParams::m)
.def_readwrite("g", &VehicleParams::g)
.def_readwrite("Crr", &VehicleParams::Crr)
.def_readwrite("rw", &VehicleParams::rw)
.def_readwrite("X", &VehicleParams::X);


py::class_<EfficiencyModel, std::shared_ptr<EfficiencyModel>>(m, "EfficiencyModel");
py::class_<StepEfficiency, EfficiencyModel,
std::shared_ptr<StepEfficiency>>(m, "StepEfficiency")
.def(py::init<>());


py::class_<ECM>(m, "ECM")
.def(py::init<const VehicleParams&, std::shared_ptr<EfficiencyModel>>())
.def("stint", &ECM::stint,
py::arg("vi"), py::arg("vt"), py::arg("t"),
py::arg("wind") = 0.0,
py::arg("slope") = 0.0,
py::arg("S") = 0.0);
}
