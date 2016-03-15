# Released under the terms of the BSD License
# (C) 2014-2016
#   Analog Devices, Inc.
#   Tim Harder <radhermit@gmail.com>
#   Ian Daniher <itdaniher@gmail.com>
#   Ezra Varady <ez@sloth.life>

import atexit
from collections import defaultdict
import operator

import _pysmu


class Smu(object):
    """TODO"""

    def __init__(self):
        atexit.register(_pysmu.cleanup)
        _pysmu.setup()

        dev_info = _pysmu.get_dev_info()
        self.serials = {i: v[0] for i, v in enumerate(dev_info)}
        self.devices = [x[1] for x in dev_info]

        names = (chr(x) for x in xrange(65,91))
        channels = {names.next(): (i, v) for i, d in enumerate(self.devices)
                    for k, v in d.iteritems()}
        self.channels = {k: Channel(k, self.serials[v[0]], v[1])
                         for k, v in channels.iteritems()}

        device_channels = defaultdict(list)
        for k, v in channels.iteritems():
            device_channels[v[0]].append(Channel(k, *v))
        self.devices = {i: Device(self.serials[i], device_channels[i])
                        for i, v in enumerate(self.devices)}

    def ctrl_transfer(self, device, bm_request_type, b_request, wValue, wIndex,
                      data, wLength, timeout):
        """TODO

        Args:
            device:
            bm_request_type:
            b_request:
            wValue:
            wIndex:
            data:
            wLength:
            timeout:

        Returns:
        """
        data = str(data)
        if bm_request_type & 0x80 == 0x80:
            if data == '0':
                data = '\x00'*wLength
        else:
            wLength = 0
        ret = _pysmu.ctrl_transfer(*args, **kwargs)
        if bm_request_type & 0x80 == 0x80:
            return map(ord, data)
        else:
            return ret

    def __repr__(self):
        return 'Devices: ' + str(self.devices)


class Device(object):
    """TODO"""

    def __init__(self, serial, channels):
        self.serial = serial
        self.channels = channels

    def get_samples(self, n_samples):
        """Query the device for a given number of samples from all channels.

        Args:
            n_samples (int): number of samples

        Returns:
            List of n samples from all the device's channels.
        """
        return _pysmu.get_all_inputs(self.serial, n_samples)

    @property
    def samples(self):
        """Iterable of samples from the device."""
        return _pysmu.iterate_inputs(self.serial)

    @property
    def calibration(self):
        """Read calibration data from the device's EEPROM."""
        return _pysmu.calibration(self.serial)

    def write_calibration(self, file):
        """Write calibration data to the device's EEPROM.

        Args:
            file (str): path to calibration file
        """
        return _pysmu.write_calibration(self.serial, file)

    def __repr__(self):
        return str(self.serial)


class Channel(object):
    """TODO"""

    def __init__(self, chan, dev_serial, signals):
        self.dev = dev_serial
        self.chan = ord(chan) - 65
        self.signals = {v: i for i, v in enumerate(signals)}

    def set_mode(self, mode):
        """TODO"""
        modes = {
            'd': 0,
            'v': 1,
            'i': 2,
        }
        mode = mode.lower()
        if mode in modes.iterkeys():
            _pysmu.set_mode(self.dev, self.chan, modes[mode])
            self.mode = mode
        else:
            raise ValueError('invalid mode: {}'.format(mode))

    def arbitrary(self, waveform, repeat=0):
        """TODO"""
        wave = map(float, reduce(operator.add, [[s]*100*n for s, n in waveform]))
        return _pysmu.set_output_buffer(wave, self.dev, self.chan, self.mode, repeat)

    def get_samples(self, n_samples):
        """Query the channel for a given number of samples.

        Args:
            n_samples (int): number of samples

        Returns:
            List of n samples from all the channel.
        """
        return _pysmu.get_inputs(self.dev, self.chan, n_samples)

    def constant(self, val):
        """Set output to a constant waveform."""
        return _pysmu.set_output_constant(self.dev, self.chan, self.mode, val)

    def square(self, midpoint, peak, period, phase, duty_cycle):
        """Set output to a square waveform."""
        return _pysmu.set_output_wave(
            self.dev, self.chan, self.mode, 1, midpoint, peak, period, phase, duty_cycle)

    def sawtooth(self, midpoint, peak, period, phase):
        """Set output to a sawtooth waveform."""
        return _pysmu.set_output_wave(
            self.dev, self.chan, self.mode, 2, midpoint, peak, period, phase, 42)

    def stairstep(self, midpoint, peak, period, phase):
        """Set output to a stairstep waveform."""
        return _pysmu.set_output_wave(
            self.dev, self.chan, self.mode, 3, midpoint, peak, period, phase, 42)

    def sine(self, midpoint, peak, period, phase):
        """Set output to a sinusoidal waveform."""
        return _pysmu.set_output_wave(
            self.dev, self.chan, self.mode, 4, midpoint, peak, period, phase, 42)

    def triangle(self, midpoint, peak, period, phase):
        """Set output to a triangle waveform."""
        return _pysmu.set_output_wave(
            self.dev, self.chan, self.mode, 5, midpoint, peak, period, phase, 42)

    def __repr__(self):
        return (
            'Device: ' + str(self.dev) + '\nChannel: ' +
            str(self.chan) + '\nSignals: ' + str(self.signals))
