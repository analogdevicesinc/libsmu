import pytest

from pysmu import Device, Session, DeviceError
from misc import prompt


# single device session fixture
@pytest.fixture(scope='module')
def session(request):
    session = Session(add_all=False)

    if session.scan() == 0:
        # no devices plugged in
        raise ValueError

    session.add(available_devices[0])
    return session

@pytest.fixture(scope='module')
def device(session):
    return session.devices[0]
