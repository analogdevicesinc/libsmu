import pytest

from pysmu import Device, Session, DeviceError, Mode

@pytest.fixture(scope='module')
def session(request):
    session = Session(add_all=False)
    session.scan()
    return session

@pytest.fixture(scope='module')
def device(session):
    return session.available_devices[0]

def test_mode(device):
    # channels start in HI_Z mode by default
    assert device.channels['A'].mode == device.channels['B'].mode == Mode.HI_Z

    # invalid mode assignment raises ValueError
    with pytest.raises(ValueError):
        device.channels['A'].mode = 4

    # raw values can't be used for assignment, enum aliases must be used
    with pytest.raises(ValueError):
        device.channels['A'].mode = 1

    device.channels['A'].mode = device.channels['B'].mode = Mode.SVMI
    assert device.channels['A'].mode == device.channels['B'].mode == Mode.SVMI
