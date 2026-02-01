from setuptools import setup, Extension
import sys
import pybind11

include_dirs = [
    pybind11.get_include(),
    "include"
]

extra_compile_args = [
    "-std=c++20",
    "-DECM_BUILD_PYTHON",
    "-fvisibility=hidden",
]

extra_link_args = []

if sys.platform == "darwin":
    extra_compile_args += ["-stdlib=libc++", "-mmacosx-version-min=10.9"]
    extra_link_args += ["-stdlib=libc++"]
elif sys.platform.startswith("linux"):
    extra_link_args += ["-Wl,--no-undefined"]

ecm_module = Extension(
    name="ecm",
    sources=[
        "python/bindings.cpp",
        "src/ecm.cpp",
        "src/efficiency.cpp",
        "src/forces.cpp",
        "src/ecm_cont/integrator.cpp",
        "src/ecm_cont/dynamics.cpp",
        "src/ecm_cont/ecm_continuous.cpp",
    ],
    include_dirs=include_dirs,
    language="c++",
    extra_compile_args=extra_compile_args,
    extra_link_args=extra_link_args,
)

setup(
    name="ecm",
    version="0.1.0",
    description="Energy Consumption Model (ECM) with Python bindings",
    ext_modules=[ecm_module],
    zip_safe=False,
    install_requires=["pybind11>=2.11"],
)

