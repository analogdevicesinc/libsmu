from __future__ import division, print_function

import sys
import time

import pytest

from pysmu import Session, Mode
from misc import prompt


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


@pytest.mark.interactive
def test_read_write_overcurrent(device):
    """Verify overcurrent detection during data acquisition."""
    if device.fwver < 2.07:
        pytest.skip("requires firmware >= 2.07")

    sys.stdout.write('\n')
    for action, value in (('loopback 2.5V to CH A', True),
                          ('loopback 5V to CH B', True),
                          ('remove loopback', False)):
        prompt(action)
        device.channels['A'].mode = Mode.SVMI
        device.channels['A'].sine(0, 5, 100, -25)
        # Output a cosine wave for channel B voltage.
        device.channels['B'].mode = Mode.SVMI
        device.channels['B'].sine(0, 5, 100, 0)
        device.get_samples(1000)
        assert device.overcurrent == value


def test_read_write_noncontinuous_fallback_values(session, device):
    """Verify fallback values are used when running out of values to write."""
    device.channels['A'].mode = Mode.SVMI
    device.channels['B'].mode = Mode.SVMI

    num_samples = 20000

    for i in range(20):
        device.channels['A'].write([(i + 2) % 6] * 1000)
        device.channels['B'].write([(i + 4) % 6] * 1000)
        samples = device.get_samples(num_samples)
        assert len(samples) == num_samples

        for sample in samples:
            assert abs(round(sample[0][0])) == (i + 2) % 6
            assert abs(round(sample[1][0])) == (i + 4) % 6


def test_read_write_continuous_large_request(session, device):
    """Request more samples than fits in the read/write queues under default settings in continuous mode.

    Internally, pysmu splits up the request into sizes smaller than the
    internal queue size, runs all the separate requests, and then returns the
    data.
    """
    device.channels['A'].mode = Mode.SVMI
    device.channels['B'].mode = Mode.SVMI
    device.write([2] * 1000, 0, cyclic=True)
    device.write([4] * 1000, 1, cyclic=True)
    session.start(0)

    samples = device.read(100000, -1)
    assert len(samples) == 100000

    for sample in samples:
        assert abs(round(sample[0][0])) == 2
        assert abs(round(sample[1][0])) == 4

    samples = device.read(100000, 100)
    assert len(samples) > 0

    for sample in samples:
        assert abs(round(sample[0][0])) == 2
        assert abs(round(sample[1][0])) == 4


def test_read_write_noncontinuous_large_request(device):
    """Request more samples than fits in the read/write queues under default settings in non-continuous mode.

    Internally, pysmu splits up the request into sizes smaller than the
    internal queue size, runs all the separate requests, and then returns the
    data.
    """
    device.channels['A'].mode = Mode.SVMI
    device.channels['B'].mode = Mode.SVMI
    device.write([2] * 1000, 0, cyclic=True)
    device.write([4] * 1000, 1, cyclic=True)

    samples = device.get_samples(100000)
    assert len(samples) == 100000

    for sample in samples:
        assert abs(round(sample[0][0])) == 2
        assert abs(round(sample[1][0])) == 4


def test_read_write_noncontinuous(session, device):
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


def test_read_write_cyclic_noncontinuous(session, device):
    device.channels['A'].mode = Mode.SVMI
    device.channels['B'].mode = Mode.SVMI

    voltage = 0
    sample_count = 0

    for _ in range(10):
        for v in range(6):
            device.write([v] * 1000, 0, cyclic=True)
            device.write([v] * 1000, 1, cyclic=True)

            samples = device.get_samples(session.queue_size + 1)

            # verify sample values
            for sample in samples:
                assert abs(round(sample[0][0])) == v
                assert abs(round(sample[1][0])) == v


def test_read_write_cyclic_continuous(session, device):
    device.channels['A'].mode = Mode.SVMI
    device.channels['B'].mode = Mode.SVMI

    voltage = 0
    sample_count = 0
    session.start(0)

    for _ in range(10):
        for v in range(6):
            device.write([v] * 1000, 0, cyclic=True)
            device.write([v] * 1000, 1, cyclic=True)

            # flush the read buffer
            #device.flush(-1, True)

            samples = device.read(session.queue_size + 1, -1)
            assert len(samples) == session.queue_size + 1

            # skipped until USB transfer flushing support is added
            ## verify sample values
            #for sample in samples:
            #    assert abs(round(sample[0][0])) == v
            #    assert abs(round(sample[1][0])) == v


@pytest.mark.long
def test_read_write_continuous_session_stop_start(session, device):
    """Test stopping and starting a continuous session in rapid succession."""
    chan_a = device.channels['A']
    chan_b = device.channels['B']
    chan_a.mode = Mode.SVMI
    chan_b.mode = Mode.SIMV
    period = 1000
    chan_a.sine(chan_a.signal.min, chan_a.signal.max, period, -(period / 4))
    chan_b.sine(chan_b.signal.min, chan_b.signal.max, period, 0)

    for _ in range(1000):
        session.start(0)
        start = time.time()
        num_samples = 0

        while True:
            num_samples += len(device.read(1000))
            if time.time() - start > 0.2:
                break

        if not num_samples:
            pytest.fail('no samples acquired')

        # Stop the session.
        session.end()

        if time.time() - start > 1:
            pytest.fail('took too long to end the continuous session')

        sys.stdout.write('*')
        sys.stdout.flush()

    sys.stdout.write('\n')


@pytest.mark.long
def test_read_write_continuity_stop_start(session, device):
    """Test switching between noncontinuous and continuous sessions in rapid succession."""
    chan_a = device.channels['A']
    chan_b = device.channels['B']
    chan_a.mode = Mode.SVMI
    chan_b.mode = Mode.SIMV
    period = 1000
    chan_a.sine(chan_a.signal.min, chan_a.signal.max, period, -(period / 4))
    chan_b.sine(chan_b.signal.min, chan_b.signal.max, period, 0)

    cont_str = {True: 'continuous', False: 'noncontinuous'}
    cont_session_interval = {True: 1, False: 2}

    for session_num in range(1000):
        # continuous sessions on odd numbers, noncontinuous on evens
        if session_num % 2:
            session.start(0)

        start = time.time()
        num_samples = 0

        while True:
            if session_num % 2:
                num_samples += len(device.read(1000))
            else:
                num_samples += len(device.get_samples(1000))
            if time.time() - start > 0.2:
                break

        if not num_samples:
            pytest.fail('no samples acquired during {} session'.format(cont_str[session.continuous]))

        # Stop the session.
        assert session.continuous == session_num % 2
        start = time.time()
        session.end()

        # allow noncontinuous sessions a little longer to bring down since the
        # explicitly wait for device completion
        if time.time() - start > cont_session_interval[session.continuous]:
            pytest.fail('took too long to end the {} session'.format(cont_str[session.continuous]))

        sys.stdout.write('*')
        sys.stdout.flush()

    sys.stdout.write('\n')


@pytest.mark.long
def test_read_write_continuous_sample_rates(session, device):
    """Verify streaming data values and speed from 100 kSPS to 10 kSPS every ~5k SPS."""
    device.channels['A'].mode = Mode.SVMI
    device.channels['B'].mode = Mode.SVMI

    sample_count = 0

    failures = []
    failure_vals = []
    failure_samples = []
    sys.stdout.write('\n')

    # Set channel output to static values for all sample rate tests.
    device.channels['A'].constant(4)
    device.channels['B'].constant(4)

    for rate in range(100, 5, -5):
        sample_count = long(0)
        failure = False
        session.sample_rate = rate * 1000
        sample_rate = session.sample_rate

        # Make sure the session got configured properly.
        if sample_rate < 0:
            print("failed to configure session: {}".format(sample_rate))
            continue

        # Verify we're within the minimum configurable range from the specified target.
        assert abs((rate * 1000) - sample_rate) <= 256
        sys.stdout.write("running test at {} SPS: ".format(sample_rate))

        start = time.time()
        session.start(0)

        while True:
            end = time.time()
            clk_diff = end - start
            # Run each session for a minute.
            if clk_diff > 60:
                break

            # Grab up to 1000 samples in a non-blocking fashion.
            samples = device.read(1000)

            # Which all should be near the expected voltage.
            for sample in samples:
                sample_count += 1
                if round(sample[0][0]) != 4:
                    failure = True
                    failure_vals.append('Chan A: {}'.format(sample[0][0]))
                    failure_samples.append(sample_count)
                if round(sample[1][0]) != 4:
                    failure = True
                    failure_vals.append('Chan B: {}'.format(sample[1][0]))
                    failure_samples.append(sample_count)

                # show output progress per second
                if sample_count % sample_rate == 0:
                    if failure:
                        failure = False
                        sys.stdout.write('#')
                    else:
                        sys.stdout.write('*')
                    sys.stdout.flush()

        sys.stdout.write('\n')

        failures.append(len(failure_samples))

        # check if bad sample values were received and display them if any exist
        if failure_samples:
            print("{} bad sample(s):".format(len(failure_samples)))
            for i, x in enumerate(failure_samples):
                print("sample: {}, expected: 4, received: {}".format(x, failure_vals[i]))

        failure_samples = []
        failure_vals = []

        samples_per_second = int(round(sample_count / clk_diff))
        # Verify we're running within 250 SPS of the configured sample rate.
        sample_rate_diff = int(samples_per_second - sample_rate)
        assert abs(sample_rate_diff) <= 250
        print("received {} samples in {} seconds: ~{} SPS ({} SPS difference)".format(
            sample_count, int(round(clk_diff)), samples_per_second, sample_rate_diff))

        # Stop the session.
        session.end()

    # fail test if any sample rates returned bad values
    assert not any(failures)
