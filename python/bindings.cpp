#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <memory>
#include "ecm/ecm.hpp"
#include "ecm_cont/ecm_continuous.hpp"
#include "ecm/efficiency.hpp"
#include "ecm_cont/integrator_dispatch.hpp"

namespace py = pybind11;

PYBIND11_MODULE(ecm, m) {
    m.doc() = "Energy Consumption Model (ECM) Python bindings";

    // ================= Enum first =================
    py::enum_<ecm_cont::IntegratorType>(m, "IntegratorType")
        .value("Euler", ecm_cont::IntegratorType::Euler)
        .value("SemiImplicitEuler", ecm_cont::IntegratorType::SemiImplicitEuler)
        .value("RK4", ecm_cont::IntegratorType::RK4)
        .export_values();

    // ================= VehicleParams =================
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
        .def_readwrite("min_eff", &ecm::VehicleParams::min_eff);

    // ================= EfficiencyModel =================
    py::class_<ecm::EfficiencyModel,
               std::shared_ptr<ecm::EfficiencyModel>>(m, "EfficiencyModel");

    py::class_<ecm::StepEfficiency,
               ecm::EfficiencyModel,
               std::shared_ptr<ecm::StepEfficiency>>(m, "StepEfficiency")
        .def(py::init<>());

    // ================= ECM (discrete) =================
    py::class_<ecm::ECM>(m, "ECM")
        .def(py::init<const ecm::VehicleParams&, std::shared_ptr<ecm::EfficiencyModel>>(),
             py::arg("params"), py::arg("efficiency").none(false))
        .def("stint", &ecm::ECM::stint,
             py::arg("vi"),
             py::arg("vt"),
             py::arg("t"),
             py::arg("wind") = 0.0,
             py::arg("slope") = 0.0,
             py::arg("S") = 570.0);

    // ================= ECMContinuous (continuous) =================
    py::class_<ecm_cont::ECMContinuous>(m, "ECMContinuous")
        // Default RK4 constructor
        .def(py::init<const ecm::VehicleParams&, const ecm::EfficiencyModel&>(),
             py::keep_alive<1, 3>(),
             py::arg("params"),
             py::arg("efficiency"))

        // Explicit integrator constructor
        .def(py::init<const ecm::VehicleParams&, const ecm::EfficiencyModel&, ecm_cont::IntegratorType>(),
             py::keep_alive<1, 3>(),
             py::arg("params"),
             py::arg("efficiency"),
             py::arg("integrator"))

        .def("stint", &ecm_cont::ECMContinuous::stint,
             py::arg("vi"),
             py::arg("vt"),
             py::arg("t"),
             py::arg("wind") = 0.0,
             py::arg("slope") = 0.0,
             py::arg("aux") = 0.0);
}

