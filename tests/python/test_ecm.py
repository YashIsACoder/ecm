import ecm

def test_basic_ecm():
    params = ecm.VehicleParams()
    params.rho = 1.225
    params.Cd = 0.1
    params.A = 1.864
    params.m = 932.8
    params.g = 9.81
    params.Crr = 0.0105
    params.rw = 0.2715
    params.X = 30.0

    eff = ecm.StepEfficiency()
    model = ecm.ECM(params, eff)

    energy = model.stint(10.0, 12.0, 1.0, 0.0, 0.0, 570)
    print("Energy:", energy)
    assert energy > 0.0
    assert energy < 1e6
    print("[Python] ECM test passed")

if __name__ == "__main__":
    test_basic_ecm()
    try:
        ecm.ECM(ecm.VehicleParams(), None)
    except TypeError:
        pass
    else:
        raise AssertionError("ECM accepted a null efficiency model")
