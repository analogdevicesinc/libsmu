import atexit

import libsmu

class pysmu(object):
    def __init__(self):
        atexit.register(libsmu.cleanup)
        libsmu.setup()
        self.load_chans()


    def load_chans(self):
        self.devices = libsmu.get_dev_info()
        names = (chr(x) for x in xrange(65,91))
        self.chans = {names.next(): (i, v) for i, d in enumerate(self.devices) for k, v in d.items()}
        self.chans = {k: channel(k, *v) for k, v in self.chans.items()}



class channel(object):
    def __init__(self, chan, dev, signals):
        self.chan = ord(chan) - 65
        self.dev = dev
        self.signals = {v: i for i, v in enumerate(signals)}


    def set_mode(self, mode):
        if mode == 'v' or mode == 'V':
            libsmu.set_mode(self.dev, self.chan, 1)
        elif mode == 'i' or mode == 'I':
            libsmu.set_mode(self.dev, self.chan, 2)
        elif mode == 'd' or mode =='D':
            libsmu.set_mode(sel.dev, self.chan, 0)
        else:
            raise Exception('invalid mode')
        self.mode = mode

    #if (!PyArg_ParseTuple(args, "iiiifffff", &dev_num, &chan_num, &mode, &wave, &val1, &val2, &period, &phase, &duty))
    #enum Src {
    #    SRC_CONSTANT,
    #    SRC_SQUARE,
    #    SRC_SAWTOOTH,
    #    SRC_SINE,
    #    SRC_TRIANGLE,
    #    SRC_BUFFER,
    #    SRC_CALLBACK,
    #};
    def constant(self, val):
        return libsmu.set_output_constant(self.dev, self.chan, self.mode, val)
    def square(midpoint, peak, period, phase, duty_cycle):
        return libsmu.output_wave(self.dev, self.chan, self.mode, 1, midpoint, peak, phase, duty_cycle)
    def sawtooth(midpoint, peak, period, phase):
        return libsmu.output_wave(self.dev, self.chan, self.mode, 2, midpoint, peak, phase, 42)
    def sine(midpoint, peak, period, phase):
        return libsmu.output_wave(self.dev, self.chan, self.mode, 3, midpoint, peak, phase, 42)
    def triangle(midpoint, peak, period, phase):
        return libsmu.output_wave(self.dev, self.chan, self.mode, 4, midpoint, peak, phase, 42)
x = pysmu()
