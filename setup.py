from pybind11.setup_helpers import Pybind11Extension, build_ext
from setuptools import setup


setup(
    name="ecm",
    version="0.1.0",
    description="Energy Consumption Model (ECM) with Python bindings",
    python_requires=">=3.8",
    ext_modules=[
        Pybind11Extension(
            "ecm",
            sources=[
                "python/bindings.cpp",
                "src/ecm.cpp",
                "src/efficiency.cpp",
                "src/efficiency_map.cpp",
                "src/forces.cpp",
                "src/segment.cpp",
                "src/ecm_cont/integrator.cpp",
                "src/ecm_cont/dynamics.cpp",
                "src/ecm_cont/ecm_continuous.cpp",
                "src/ecm_cont/drive_cycle.cpp",
            ],
            include_dirs=["include"],
            cxx_std=20,
        )
    ],
    cmdclass={"build_ext": build_ext},
    zip_safe=False,
)
