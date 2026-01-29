import ecm

params = ecm.VehicleParams()
params.X = 30.0

eff = ecm.StepEfficiency()

disc = ecm.ECM(params, eff)
cont = ecm.ECMContinuous(params, eff)

E_d = disc.stint(10, 12, 1.0, 0.0, 0.0, 570.0)
E_c = cont.stint(10, 12, 1.0, 0.0, 0.0, params.X - 570.0)

print(E_d, E_c)

