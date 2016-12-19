#!/usr/bin/env python
#
# Simple script showing how to stream data from a device in noncontinuous mode,
# use Ctrl-C to exit.

from __future__ import print_function

from signal import signal, SIG_DFL, SIGINT
import sys

from pysmu import Session, Mode


# If stdout is a terminal continuously overwrite a single line, otherwise
# output each line individually.
if sys.stdout.isatty():
    output = lambda s: sys.stdout.write("\r" + s)
else:
    output = print


if __name__ == '__main__':
    # don't throw KeyboardInterrupt on Ctrl-C
    signal(SIGINT, SIG_DFL)

    # only add one device to the session
    session = Session(add_all=False)
    session.scan()
    session.add(session.available_devices[0])

    if session.devices:
        while True:
            # Run the session for 1024 samples in noncontinuous mode and read
            # incoming samples from both channels of the first device in a
            # blocking fashion.
            samples = session.get_samples(1024)[0]
            for x in samples:
                output("{: 6f} {: 6f} {: 6f} {: 6f}".format(x[0][0], x[0][1], x[1][0], x[1][1]))
    else:
        print('no devices attached')
