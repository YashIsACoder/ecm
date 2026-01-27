import ecm

def test_basic_ecm():
    params = ecm.VehicleParams(
        rho=1.225,
        Cd=0.1,
        A=1.864,
        m=932.8,
        g=9.81,
        Crr=0.0105,
        rw=0.2715
    )

    eff = ecm.StepEfficiency()
    model = ecm.ECM(params, eff)

    energy = model.stint(
        t=1.0,
        vi=10.0,
        vt=12.0,
        S=0.0,
        X=500.0
    )

    print("Energy:", energy)

    assert energy > 0.0
    assert energy < 1e6

    print("[Python] ECM stint test passed")


if __name__ == "__main__":
    test_basic_ecm()

