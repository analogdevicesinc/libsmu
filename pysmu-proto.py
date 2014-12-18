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

pysmu()
