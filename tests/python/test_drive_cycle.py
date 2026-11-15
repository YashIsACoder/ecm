import math

import ecm


def test_profile():
    vehicle = ecm.VehicleParams()
    vehicle.Cd = 0.0
    vehicle.Crr = 0.0
    vehicle.m = 200.0
    vehicle.X = 50.0
    profile = [
        ecm.SpeedInterval(10.0, 0.0, 10.0, irradiance_w_m2=1000.0),
        ecm.SpeedInterval(10.0, 10.0, 0.0),
    ]
    drivetrain = ecm.Drivetrain(0.8, 0.5, 10000.0)
    array = ecm.SolarArray(4.0, 0.20, 0.90)
    result = ecm.drive_cycle(vehicle, profile, drivetrain, array)
    assert math.isclose(result.distance_m, 100.0)
    assert math.isclose(result.wheel_wh, 0.0, abs_tol=1e-12)
    assert math.isclose(result.traction_wh, 7500.0 / 3600.0)
    assert math.isclose(result.recovered_wh, 5000.0 / 3600.0)
    assert math.isclose(result.auxiliary_wh, 1000.0 / 3600.0)
    assert math.isclose(result.solar_wh, 2.0)
    assert math.isclose(result.net_dc_wh, 13.0 / 36.0)

    try:
        ecm.drive_cycle(vehicle, [], drivetrain)
    except ValueError:
        pass
    else:
        raise AssertionError("accepted an empty drive cycle")


if __name__ == "__main__":
    test_profile()
