import pytest

from pysmu import Device, Session, DeviceError, Mode

@pytest.fixture(scope='function')
def session(request):
    s = Session()
    yield s

    # force session destruction
    s._close()

@pytest.fixture(scope='function')
def device(session):
    return session.devices[0]

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

def test_channel_read(session, device):
    session.run(1000)
    samples = device.channels['A'].read(1000, -1)
    assert len(samples) == 1000
    assert len(samples[0]) == 2

def test_channel_write(device):
    pass

def test_get_samples(device):
    samples = device.channels['A'].get_samples(1000)
    assert len(samples) == 1000
    assert len(samples[0]) == 2

def test_arbitrary(device):
    pass

def test_constant(session, device):
    device.channels['A'].mode = Mode.SVMI
    device.channels['A'].constant(2)
    device.channels['B'].mode = Mode.SVMI
    device.channels['B'].constant(4)

    # verify sample values are near 2 for channel A
    samples = device.channels['A'].get_samples(1000)
    assert len(samples) == 1000
    for x in samples:
        assert abs(round(x[0])) == 2

    # verify sample values are near 4 for channel B
    samples = device.channels['B'].get_samples(1000)
    assert len(samples) == 1000
    for x in samples:
        assert abs(round(x[0])) == 4
