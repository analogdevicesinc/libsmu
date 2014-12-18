import atexit

import libsmu

class pysmu(object):
    def __init__(self):
        atexit.register(libsmu.cleanup)
        libsmu.setup()
        self.devices = libsmu.get_dev_info()

