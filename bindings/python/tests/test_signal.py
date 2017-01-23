import pytest

from pysmu import Session


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


def test_constant(signal):
    data = signal.constant(100, 4)
    assert len(data) == 100
    for x in data:
        assert x == 4


def test_square(signal):
    data = signal.square(100, 0, 5, 99, 0, 0)
    assert len(data) == 100


def test_sawtooth(signal):
    data = signal.sawtooth(100, -5, 5, 99, 0)
    assert len(data) == 100


def test_stairstep(signal):
    data = signal.stairstep(100, -5, 5, 99, 0)
    assert len(data) == 100


def test_sine(signal):
    data = signal.sine(100, -5, 5, 99, 0)
    assert len(data) == 100


def test_triangle(signal):
    data = signal.triangle(100, -5, 5, 99, 0)
    assert len(data) == 100
