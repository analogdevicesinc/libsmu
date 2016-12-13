#!/usr/bin/env python
#
# Simple script showing how to read and write data to a device in noncontinuous
# mode, use Ctrl-C to exit.

from __future__ import print_function

from signal import signal, SIG_DFL, SIGINT
import sys
import random

from pysmu import Session, Mode


# If stdout is a terminal continuously overwrite a single line, otherwise
# output each line individually.
if sys.stdout.isatty():
    output = lambda s: sys.stdout.write("\r" + s)
else:
    output = print


def refill_data(num_samples, v=None):
    if v is None:
        # fill channels with a static, random integer between 0 and 5
        v = random.randint(0, 5)
    return [v] * num_samples
    # fill channels with a static, random float between -0.2 and 0.2
    #return [random.uniform(-0.2,0.2)] * num_samples


if __name__ == '__main__':
    # don't throw KeyboardInterrupt on Ctrl-C
    signal(SIGINT, SIG_DFL)

    session = Session()

    if session.devices:
        # Grab the first device from the session.
        dev = session.devices[0]

        # Ignore read buffer overflows when printing to stdout.
        dev.ignore_dataflow = sys.stdout.isatty()

        # Set both channels to source voltage, measure current mode.
        chan_a = dev.channels['A']
        chan_b = dev.channels['B']
        chan_a.mode = Mode.SVMI
        chan_b.mode = Mode.SVMI
        #chan_a.mode = Mode.SIMV
        #chan_b.mode = Mode.SIMV

        i = 0
        num_samples = 1024

        while True:
            # If stdout is a terminal change the written value every ten
            # iterations, otherwise change it on every iteration.
            if sys.stdout.isatty():
                v = i / 10
            else:
                v = i

            # Write iterating voltage values to both channels.
            chan_a.write(refill_data(num_samples, v % 6))
            chan_b.write(refill_data(num_samples, v % 6))
            i += 1

            session.run(num_samples)

            # Read incoming samples in a blocking fashion.
            samples = dev.read(num_samples, -1)
            for x in samples:
                output("{: 6f} {: 6f} {: 6f} {: 6f}".format(x[0][0], x[0][1], x[1][0], x[1][1]))
    else:
        print('no devices attached')
