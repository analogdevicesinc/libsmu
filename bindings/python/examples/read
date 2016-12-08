#!/usr/bin/env python
#
# Simple script showing how to stream data from a device, use Ctrl-C to exit.

from __future__ import print_function

from signal import signal, SIG_DFL, SIGINT
import sys

from pysmu import Session, Mode, BufferOverflow


if __name__ == '__main__':
    # don't throw KeyboardInterrupt on Ctrl-C
    signal(SIGINT, SIG_DFL)

    session = Session()

    if session.devices:
        dev = session.devices[0]
        dev.channels['A'].mode = Mode.HI_Z
        dev.channels['B'].mode = Mode.HI_Z
        session.start(0)
        ignore_overflow = sys.stdout.isatty()
        while True:
            # Ignore read buffer overflows when printing to stdout.
            samples = dev.read(1000, -1, ignore_overflow=ignore_overflow)
            for x in samples:
                print("{:6f} {:6f} {:6f} {:6f}".format(x[0][0], x[0][1], x[1][0], x[1][1]))
    else:
        print('no devices attached')
