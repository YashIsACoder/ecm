from setuptools import setup, Extension
import sys
import os
import pybind11

# Helper: automatically add pybind11 include
include_dirs = [
    pybind11.get_include(),
    "include"
]

# Extra compile flags
extra_compile_args = ["-std=c++20"]

# Add macOS specific flags for shared library symbols
if sys.platform == "darwin":
    extra_compile_args += ["-stdlib=libc++", "-mmacosx-version-min=10.9"]

# Define the Python extension module
ecm_module = Extension(
    name="ecm",
    sources=[
        "python/bindings.cpp",   # pybind11 bindings
        "src/ecm.cpp",           # ECM implementation
        "src/efficiency.cpp",    # efficiency models
        "src/forces.cpp"         # force calculations
    ],
    include_dirs=include_dirs,
    language="c++",
    extra_compile_args=extra_compile_args,
)

setup(
    name="ecm",
    version="0.1",
    author="Yashwanth Madan",
    description="Energy Consumption Model (ECM) with Python bindings",
    ext_modules=[ecm_module],
    zip_safe=False,
    install_requires=["pybind11>=2.10"],  # ensures pybind11 is installed
)
