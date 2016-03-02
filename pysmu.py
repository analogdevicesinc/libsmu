# Released under the terms of the BSD License
# (C) 2014-2016
#   Analog Devices, Inc.
#   Tim Harder <radhermit@gmail.com>
#   Ian Daniher <itdaniher@gmail.com>
#   Ezra Varady <ez@sloth.life>

import atexit
from collections import defaultdict
import operator

import libpysmu


class Smu(object):

    def __init__(self):
        atexit.register(libpysmu.cleanup)
        libpysmu.setup()

        dev_info = libpysmu.get_dev_info()
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
        data = str(data)
        if bm_request_type & 0x80 == 0x80:
            if data == '0':
                data = '\x00'*wLength
        else:
            wLength = 0
        ret = libpysmu.ctrl_transfer(*args, **kwargs)
        if bm_request_type & 0x80 == 0x80:
            return map(ord, data)
        else:
            return ret

    def __repr__(self):
        return 'Devices: ' + str(self.devices)


class Device(object):
    def __init__(self, serial, channels):
        self.serial = serial
        self.channels = channels

    def get_samples(self, n_samples):
        """query device for a list of samples from all channels

        :param n_samples: number of samples
        :type n_samples: int
        :return: list of n samples from all the device's channels
        """
        return libpysmu.get_all_inputs(self.serial, n_samples)

    @property
    def samples(self):
        """iterator for samples from the device run in continuous mode"""
        return libpysmu.iterate_inputs(self.serial)

    def __repr__(self):
        return str(self.serial)


class Channel(object):
    def __init__(self, chan, dev_serial, signals):
        self.dev = dev_serial
        self.chan = ord(chan) - 65
        self.signals = {v: i for i, v in enumerate(signals)}

    def set_mode(self, mode):
        modes = {
            'd': 0,
            'v': 1,
            'i': 2,
        }
        mode = mode.lower()
        if mode in modes.iterkeys():
            libpysmu.set_mode(self.dev, self.chan, modes[mode])
            self.mode = mode
        else:
            raise ValueError('invalid mode: {}'.format(mode))

    def arbitrary(self, waveform, repeat=0):
        wave = map(float, reduce(operator.add, [[s]*100*n for s, n in waveform]))
        return libpysmu.set_output_buffer(wave, self.dev, self.chan, self.mode, repeat)

    def get_samples(self, n_samples):
        """query channel for samples

        :param n_samples: number of samples
        :type n_samples: int
        :return: list of n samples from the channel
        """
        return libpysmu.get_inputs(self.dev, self.chan, n_samples)

    def constant(self, val):
        """set output to a constant waveform"""
        return libpysmu.set_output_constant(self.dev, self.chan, self.mode, val)

    def square(self, midpoint, peak, period, phase, duty_cycle):
        """set output to a square waveform"""
        return libpysmu.set_output_wave(
            self.dev, self.chan, self.mode, 1, midpoint, peak, period, phase, duty_cycle)

    def sawtooth(self, midpoint, peak, period, phase):
        """set output to a sawtooth waveform"""
        return libpysmu.set_output_wave(
            self.dev, self.chan, self.mode, 2, midpoint, peak, period, phase, 42)

    def stairstep(self, midpoint, peak, period, phase):
        """set output to a stairstep waveform"""
        return libpysmu.set_output_wave(
            self.dev, self.chan, self.mode, 3, midpoint, peak, period, phase, 42)

    def sine(self, midpoint, peak, period, phase):
        """set output to a sinusoidal waveform"""
        return libpysmu.set_output_wave(
            self.dev, self.chan, self.mode, 4, midpoint, peak, period, phase, 42)

    def triangle(self, midpoint, peak, period, phase):
        """set output to a triangle waveform"""
        return libpysmu.set_output_wave(
            self.dev, self.chan, self.mode, 5, midpoint, peak, period, phase, 42)

    def __repr__(self):
        return (
            'Device: ' + str(self.dev) + '\nChannel: ' +
            str(self.chan) + '\nSignals: ' + str(self.signals))


if __name__ == '__main__':
    x = Smu()
    if x.devices:
        A = x.channels['A']
        A.set_mode('v')
        A.constant(3)
        #A.arbitrary((5, 400), (2.5, 400))
        #print A.sine(1, 3, 5, 0)
        #print A.square(0, 3, 10, 0, .5)
        #print A.triangle(2,3,10,0)
        print A.get_samples(1000)

        d = x.devices[0]
        print(d.get_samples(10))
        import itertools
        print(list(itertools.islice(d.samples, 10)))
