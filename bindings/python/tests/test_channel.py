from __future__ import division

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
def chan_a(device):
    return device.channels['A']


@pytest.fixture(scope='function')
def chan_b(device):
    return device.channels['B']


def test_mode(chan_a, chan_b):
    # channels start in HI_Z mode by default
    assert chan_a.mode == chan_b.mode == Mode.HI_Z

    # invalid mode assignment raises ValueError
    with pytest.raises(ValueError):
        chan_a.mode = 4

    # raw values can't be used for assignment, enum aliases must be used
    with pytest.raises(ValueError):
        chan_a.mode = 1

    chan_a.mode = chan_b.mode = Mode.SVMI
    assert chan_a.mode == chan_b.mode == Mode.SVMI


def test_channel_read(session, chan_a):
    session.run(1000)
    samples = chan_a.read(1000, -1)
    assert len(samples) == 1000
    assert len(samples[0]) == 2


def test_channel_write(chan_a, chan_b):
    pass


def test_get_samples(chan_a, chan_b):
    samples = chan_a.get_samples(1000)
    assert len(samples) == 1000
    assert len(samples[0]) == 2


def test_arbitrary(chan_a, chan_b):
    pass


def test_chan_constant(chan_a, chan_b):
    chan_a.mode = Mode.SVMI
    chan_a.constant(2)
    chan_b.mode = Mode.SVMI
    chan_b.constant(4)

    # verify sample values are near 2 for channel A
    samples = chan_a.get_samples(1000)
    assert len(samples) == 1000
    for x in samples:
        assert abs(round(x[0])) == 2

    # verify sample values are near 4 for channel B
    samples = chan_b.get_samples(1000)
    assert len(samples) == 1000
    for x in samples:
        assert abs(round(x[0])) == 4


def test_chan_sine(chan_a, chan_b, device):
    for freq in (10, 25, 50):
        period = freq * 10
        num_samples = period * freq

        # write a sine wave to both channels, one as a voltage source and the
        # other as current
        chan_a.mode = Mode.SVMI
        chan_a.sine(chan_a.signal.min, chan_a.signal.max, period, 0)
        chan_b.mode = Mode.SIMV
        chan_b.sine(chan_b.signal.min, chan_b.signal.max, period, 0)

        # additional sample so we end up back at the starting value for a sine wave
        samples = device.get_samples(num_samples + 1)
        chan_a_samples = [x[0][0] for x in samples]
        chan_b_samples = [x[1][1] for x in samples]
        assert len(chan_a_samples) == len(chan_b_samples) == num_samples + 1

        try:
            # Verify the frequencies of the resulting waveforms
            import numpy as np
            from scipy import signal
            hanning = signal.get_window('hanning', num_samples)
            chan_a_freqs, chan_a_psd = signal.welch(chan_a_samples, window=hanning, nperseg=num_samples)
            chan_b_freqs, chan_b_psd = signal.welch(chan_b_samples, window=hanning, nperseg=num_samples)
            assert np.argmax(chan_a_psd) == np.argmax(chan_b_psd)
            assert abs(freq - np.argmax(chan_a_psd)) <= 1
        except ImportError:
            pass
