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
git clone <repo_url>
cd ECM_module
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
    slope=0.0,# slope angle (radians)
    S=570     # auxiliary energy offset (W)
)

print("Energy consumed:", energy)
```

---

## C++ Usage

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

MIT License — see LICENSE file.


