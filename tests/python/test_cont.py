import math
import weakref

import ecm

params = ecm.VehicleParams()
params.X = 30.0

eff = ecm.StepEfficiency()

disc = ecm.ECM(params, eff)
cont = ecm.ECMContinuous(params, eff)
cont_euler = ecm.ECMContinuous(
    params,
    eff,
    integrator=ecm.IntegratorType.Euler
)

E_d  = disc.stint(10, 12, 1.0, 0.0, 0.0, 570.0)
E_c  = cont.stint(10, 12, 1.0, 0.0, 0.0, params.X - 570.0)
E_c1 = cont_euler.stint(10, 12, 1.0, 0.0, 0.0, params.X - 570.0)

print("Discrete:", E_d)
print("Continuous (RK4):", E_c)
print("Continuous (Euler):", E_c1)


def test_efficiency_lifetime():
    for options in ({}, {"integrator": ecm.IntegratorType.Euler}):
        efficiency = ecm.StepEfficiency()
        efficiency_ref = weakref.ref(efficiency)
        model = ecm.ECMContinuous(params, efficiency, **options)
        del efficiency

        assert efficiency_ref() is not None
        assert math.isfinite(model.stint(10.0, 12.0, 1.0))

        del model
        assert efficiency_ref() is None


if __name__ == "__main__":
    test_efficiency_lifetime()

