from itertools import chain
import sys
import time

import pytest

from pysmu import Device, Session, DeviceError, SampleDrop
from misc import prompt


# single device session fixture
@pytest.fixture(scope='module')
def session(request):
    session = Session(add_all=False)

    if session.scan() == 0:
        # no devices plugged in
        raise ValueError

    session.add(session.available_devices[0])
    return session

@pytest.fixture(scope='module')
def device(session):
    return session.devices[0]

def test_read_non_continuous(session, device):
    """verify streaming HI-Z data values for ~10 seconds"""

    start = time.time()
    sample_count = 0

    while True:
        end = time.time()
        if end - start > 10:
            break

        session.run(1000)
        samples = device.read(1000, -1)
        assert len(samples) == 1000
        sample_count += 1000

        # progress updates
        if sample_count > session.sample_rate:
            sample_count = 0
            sys.stdout.write('*')
            sys.stdout.flush()

        # verify all samples are near 0
        for sample in samples:
            for x in chain.from_iterable(sample):
                assert abs(round(x)) == 0

def test_read_continuous_dataflow_ignore(session, device):
    """Verify workflows that lead to data flow issues are ignored by default."""
    session.start(0)
    time.sleep(.5)
    # by default, data flow issues are ignored so no exception should be raised here
    samples = device.read(1000)
    assert len(samples) == 1000

def test_read_continuous_dataflow_raises():
    """Verify workflows that lead to data flow issues."""
    # create a session that doesn't ignore data flow issues
    session = Session(add_all=False, ignore_dataflow=False)
    session.scan()
    session.add(session.available_devices[0])
    device = session.devices[0]

    session.start(0)
    time.sleep(.5)
    with pytest.raises(SampleDrop):
        samples = device.read(1000)

def test_read_continuous_large_request(session, device):
    """Request more samples than fits in the read/write queues under default settings in continuous mode."""
    session.start(0)

    samples = device.read(100000, -1)
    assert len(samples) == 100000

    samples = device.read(100000, 100)
    assert len(samples) > 0


def test_read_non_continuous_large_request(device):
    """Request more samples than fits in the read/write queues under default settings in non-continuous mode.

    Internally, pysmu splits up the request into sizes smaller than the
    internal queue size, runs all the separate requests, and then returns the
    data.
    """
    samples = device.get_samples(100000)
    assert len(samples) == 100000
