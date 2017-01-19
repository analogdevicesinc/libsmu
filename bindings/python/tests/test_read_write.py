from itertools import chain
import sys
import time

import pytest

from pysmu import Device, Session, Mode


# single device session fixture
@pytest.fixture(scope='function')
def session(request):
    s = Session(add_all=False)

    if s.scan() == 0:
        # no devices plugged in
        raise ValueError

    s.add(s.available_devices[0])
    yield s

    # force session destruction
    s._close()

@pytest.fixture(scope='function')
def device(session):
    return session.devices[0]

def test_read_write_non_continuous_fallback_values(session, device):
    """Verify fallback values are used when running out of values to write."""
    device.channels['A'].mode = Mode.SVMI
    device.channels['B'].mode = Mode.SVMI

    num_samples = 20000
    device.write([2]*1000, 0)
    device.write([4]*1000, 1)
    samples = device.get_samples(num_samples)

    assert len(samples) == num_samples

    for sample in samples:
        assert abs(round(sample[0][0])) == 2
        assert abs(round(sample[1][0])) == 4
