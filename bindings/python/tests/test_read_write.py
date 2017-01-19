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

def test_read_write_non_continuous(session, device):
    device.channels['A'].mode = Mode.SVMI
    device.channels['B'].mode = Mode.SVMI

    voltage = 0
    sample_count = 0
    start = time.time()

    # Verify read/write data for 10 seconds.
    while True:
        end = time.time()
        clk_diff = end - start
        # Run each session for a minute.
        if clk_diff > 10:
            break

        # Write iterating voltage values to both channels.
        device.write([voltage % 6] * 1000, 0)
        device.write([voltage % 6] * 1000, 1)

        # Run the session for 1000 samples.
        session.run(1000)

        # Read incoming samples in a blocking fashion.
        samples = device.read(1000, -1)
        assert len(samples) == 1000

        # Validate received values.
        for sample in samples:
            sample_count += 1
            assert abs(round(sample[0][0])) == voltage % 6
            assert abs(round(sample[1][0])) == voltage % 6

            # show output progress per second
            if sample_count % session.sample_rate == 0:
                sys.stdout.write('*')
                sys.stdout.flush()

        voltage += 1

    sys.stdout.write('\n')

def test_read_write_continuous(session, device):
    device.channels['A'].mode = Mode.SVMI
    device.channels['B'].mode = Mode.SVMI

    voltage = 0
    sample_count = 0
    start = time.time()
    session.start(0)

    # Verify read/write data for 10 seconds.
    while True:
        end = time.time()
        clk_diff = end - start
        # Run each session for a minute.
        if clk_diff > 10:
            break

        # Write iterating voltage values to both channels.
        device.write([voltage % 6] * 1000, 0)
        device.write([voltage % 6] * 1000, 1)

        # Read incoming samples in a non-blocking fashion.
        samples = device.read(1000)

        # Validate received values.
        for sample in samples:
            sample_count += 1
            assert abs(round(sample[0][0])) == voltage % 6
            assert abs(round(sample[1][0])) == voltage % 6

            # show output progress per second
            if sample_count % session.sample_rate == 0:
                sys.stdout.write('*')
                sys.stdout.flush()

        if sample_count and sample_count % 1000 == 0:
            voltage += 1

    sys.stdout.write('\n')

    # Verify we're running near the set sample rate.
    assert abs(round(sample_count / 10) - session.sample_rate) <= 256
