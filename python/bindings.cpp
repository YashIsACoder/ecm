#include <pybind11/pybind11.h>
#include <memory>
#include "ecm/ecm.hpp"
#include "ecm_cont/ecm_continuous.hpp"
#include "ecm/efficiency.hpp"

namespace py = pybind11;

PYBIND11_MODULE(ecm, m) {
    m.doc() = "Energy Consumption Model (ECM) Python bindings";

    // VehicleParams exposed to Python
    py::class_<ecm::VehicleParams>(m, "VehicleParams")
        .def(py::init<>())
        .def_readwrite("rho", &ecm::VehicleParams::rho)
        .def_readwrite("Cd", &ecm::VehicleParams::Cd)
        .def_readwrite("A", &ecm::VehicleParams::A)
        .def_readwrite("m", &ecm::VehicleParams::m)
        .def_readwrite("g", &ecm::VehicleParams::g)
        .def_readwrite("Crr", &ecm::VehicleParams::Crr)
        .def_readwrite("rw", &ecm::VehicleParams::rw)
        .def_readwrite("X", &ecm::VehicleParams::X)
        .def_readwrite("min_eff", &ecm::VehicleParams::min_eff)
        ;

    // EfficiencyModel base class
    py::class_<ecm::EfficiencyModel,
               std::shared_ptr<ecm::EfficiencyModel>>(m, "EfficiencyModel");

    // StepEfficiency concrete class
    py::class_<ecm::StepEfficiency,
               ecm::EfficiencyModel,
               std::shared_ptr<ecm::StepEfficiency>>(m, "StepEfficiency")
        .def(py::init<>());

    // ECM model
    py::class_<ecm::ECM>(m, "ECM")
        .def(py::init<const ecm::VehicleParams&, std::shared_ptr<ecm::EfficiencyModel>>())
        .def("stint", &ecm::ECM::stint,
             py::arg("vi"),
             py::arg("vt"),
             py::arg("t"),
             py::arg("wind") = 0.0,
             py::arg("slope") = 0.0,
             py::arg("S") = 570.0 
        );

    // Continuous ECM model
    py::class_<ecm_cont::ECMContinuous>(m, "ECMContinuous")
        .def(py::init<const ecm::VehicleParams&,
                      const ecm::EfficiencyModel&>(),
             py::keep_alive<1, 2>(),   // keep EfficiencyModel alive
             py::arg("params"),
             py::arg("efficiency"))
        .def("stint", &ecm_cont::ECMContinuous::stint,
             py::arg("vi"),
             py::arg("vt"),
             py::arg("t"),
             py::arg("wind") = 0.0,
             py::arg("slope") = 0.0,
             py::arg("aux") = 0.0
        );
}

