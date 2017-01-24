import pytest

from pysmu import Session, Mode


@pytest.fixture(scope='function')
def session(request):
    s = Session()
    yield s

    # force session destruction
    s._close()


@pytest.fixture(scope='function')
def device(session):
    return session.devices[0]


@pytest.fixture(scope='function')
def device(session):
    return session.devices[0]


@pytest.fixture(scope='function')
def chan_a(device):
    return device.channels['A']


@pytest.fixture(scope='function')
def chan_b(device):
    return device.channels['B']


@pytest.fixture(scope='function')
def signal(chan_a):
    return chan_a.signal


def test_signal_info(chan_a):
    chan_a.mode = Mode.SVMI
    assert chan_a.signal.label == 'Voltage'
    assert chan_a.signal.min == 0
    assert chan_a.signal.max == 5
    chan_a.mode = Mode.SIMV
    assert chan_a.signal.label == 'Current'
    assert chan_a.signal.min == -0.2
    assert chan_a.signal.max == 0.2


def test_constant(signal):
    data = signal.constant(100, 4)
    assert len(data) == 100
    for x in data:
        assert x == 4


def test_square(signal):
    data = signal.square(101, 0, 5, 100, 0, 0.5)
    assert len(data) == 101
    for i in range(50):
        assert data[i] == 0
        assert data[i + 50] == 5
    assert data[100] == 0


def test_sawtooth(signal):
    data = signal.sawtooth(101, -5, 5, 100, 0)
    assert data[0] == 5
    assert data[99] == -5
    assert data[100] == 5
    assert len(data) == 101


def test_stairstep(signal):
    data = signal.stairstep(101, -5, 5, 100, 0)
    assert len(data) == 101
    assert data[0] == 5
    assert data[100] == 5

    for i in range(10):
        step_vals = [data[i * 10 + j] for j in range(10)]
        check_val = step_vals[0]
        for i, x in enumerate(step_vals):
            assert x == check_val


def test_sine(signal):
    data = signal.sine(101, -5, 5, 100, -25)
    assert len(data) == 101
    assert round(data[0], 4) == 0
    assert round(data[25], 4) == 5
    assert round(data[50], 4) == 0
    assert round(data[75], 4) == -5
    assert round(data[100], 4) == 0


def test_triangle(signal):
    data = signal.triangle(101, -5, 5, 100, 0)
    assert len(data) == 101
    assert data[0] == 5
    assert data[50] == -5
    assert data[100] == 5
