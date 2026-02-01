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

