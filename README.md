# ECM — Energy Consumption Model

**ECM** is a lightweight, flexible, and portable C++ Energy Consumption Model library with Python bindings via pybind11. It allows users to simulate energy consumption for vehicles based on velocity, forces, and efficiency models.

The library is suitable for **simulations, testing, and research**, and can be pip-installed for Python use.

---

## Features

* **C++ core** for high-performance simulations
* **Python bindings** via pybind11 (`import ecm`)
* Flexible **efficiency models** (e.g., stepwise efficiency)
* Discrete **stint-based energy calculations**
* Easy **vehicle parameter customization**
* Pip-installable on **macOS, Linux, Windows**

---

## Installation

**Requirements:**

* Python 3.8+
* C++20 compatible compiler
* `pybind11` (automatically handled via pip)

**Install via pip (from source):**

```bash
git clone git@github.com:YashIsACoder/ecm.git
cd ecm
pip install .
```

> This will build the C++ library and Python module automatically.

---

## Usage (Python)

```python
import ecm

# Define vehicle parameters
params = ecm.VehicleParams()
params.rho = 1.225
params.Cd = 0.1
params.A = 1.864
params.m = 932.8
params.g = 9.81
params.Crr = 0.0105
params.rw = 0.2715
params.X = 500.0  # auxiliary power

# Choose efficiency model
eff = ecm.StepEfficiency()

# Create ECM model
model = ecm.ECM(params, eff)

# Compute energy consumed in a stint
energy = model.stint(
    vi=10.0,  # initial velocity (m/s)
    vt=12.0,  # target velocity (m/s)
    t=1.0,    # time period (s)
    wind=0.0, # wind speed (m/s)
    slope=0.0,# grade as rise divided by horizontal run
    S=570     # solar power (W)
)

print("Energy consumed:", energy)
```

---

## C++ Usage

For a dependency-free C++ build:

```sh
cmake -S . -B build -DECM_BUILD_PYTHON=OFF -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cmake --install build --prefix "$PWD/build/install"
```

Link `ECM::ecm` for the discrete and road-segment APIs or `ECM::ecm_cont` for the continuous model. Both targets are available with `add_subdirectory` and via `find_package(ECM 0.1 CONFIG REQUIRED)` using the install prefix in `CMAKE_PREFIX_PATH`.

### Discrete stint

```cpp
#include "ecm/ecm.hpp"
#include "ecm/efficiency.hpp"
#include <memory>
#include <iostream>

int main() {
    ecm::VehicleParams params;
    params.X = 500.0;

    auto eff = std::make_shared<ecm::StepEfficiency>();
    ecm::ECM model(params, eff);

    double energy = model.stint(10.0, 12.0, 1.0, 0.0, 0.0, 570);
    std::cout << "Energy consumed: " << energy << std::endl;
    return 0;
}
```

### Road-segment energy and solar input

`ecm/segment.hpp` provides a speed-and-distance API for batch route experiments. It reuses the library's force calculations and reports motion, traction and auxiliary energy separately, in watt-hours. The segment model cruises at one speed and accounts for net kinetic energy from entry to exit. The drivetrain has separate drive and regeneration efficiencies and a cap on recovered electrical power.

```cpp
#include <ecm/segment.hpp>

const ecm::VehicleParams car{
    .rho = 1.2, .Cd = 0.14, .A = 1.0, .m = 200, .Crr = 0.005, .X = 60,
};
const ecm::RoadSegment road{36'000, 20, 20, 20, 0, 0};
const ecm::Drivetrain drivetrain{0.90, 0.55, 100};
const ecm::SolarArray array{4, 0.20, 0.90};
const auto drive = ecm::drive_energy(car, road, drivetrain);
const double net_draw_wh = drive.demand_wh() - ecm::solar_energy_wh(array, 1000, drive.seconds);
```

A positive grade is rise divided by horizontal run (`0.01` is a 1% climb). Positive wind opposes travel. For the segment API, the library reports solar separately from vehicle demand so a caller can apply its own battery storage limits. The existing `stint()` APIs return joules.

---

## Project Structure

```
ECM_module/
├─ include/ecm/       # C++ headers
├─ src/               # C++ source files
├─ python/            # pybind11 bindings
├─ tests/             # Python and C++ tests
├─ CMakeLists.txt     # build configuration
├─ setup.py           # pip install setup
├─ pyproject.toml     # modern PEP517 build
└─ README.md
```

---

## Testing

### Python test

```bash
python tests/test_ecm.py
```

### C++ test

```bash
cd build
./ecm_test
```

> Ensure C++ test is built via CMake (`cmake .. && cmake --build .`) before running.

---

## Notes

* `VehicleParams` contains static vehicle parameters.
* `stint()` computes **energy consumed in a discrete time period**, taking `vi`, `vt`, `t`, `wind`, `slope`, and `S` as inputs.
* Efficiency models can be extended by subclassing `EfficiencyModel`.

---

## License

See the [MIT License](LICENSE.md).


