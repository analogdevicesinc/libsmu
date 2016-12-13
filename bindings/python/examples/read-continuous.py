#!/usr/bin/env python
#
# Simple script showing how to stream data from a device in continuous mode,
# use Ctrl-C to exit.

from __future__ import print_function

from signal import signal, SIG_DFL, SIGINT
import sys

from pysmu import Session, Mode


if __name__ == '__main__':
    # don't throw KeyboardInterrupt on Ctrl-C
    signal(SIGINT, SIG_DFL)

    session = Session()

    if session.devices:
        # Grab the first device from the session.
        dev = session.devices[0]

        # Ignore read buffer overflows when printing to stdout.
        dev.ignore_dataflow = sys.stdout.isatty()

        # Start a continuous session.
        session.start(0)

        while True:
            # Read incoming samples from both channels which are in HI-Z mode
            # by default in a blocking fashion.
            samples = dev.read(1024, -1)
            for x in samples:
                print("{:6f} {:6f} {:6f} {:6f}".format(x[0][0], x[0][1], x[1][0], x[1][1]))
    else:
        print('no devices attached')
