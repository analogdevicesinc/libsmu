#!/usr/bin/env python
#
# Simple script showing how to get a set number of samples from all devices
# connected to a system.

from __future__ import print_function

import sys

from pysmu import Session, Mode


if __name__ == '__main__':
    session = Session()

    if not session.devices:
        sys.exit(1)

    for idx, dev in enumerate(session.devices):
        # Set all devices in the session to use source voltage, measure current
        # mode for channel A with a constant value based on their index.
        dev.channels['A'].mode = Mode.SVMI
        dev.channels['A'].constant(idx % 6)
        # Set all devices in the session to use source current, measure voltage
        # mode for channel B with a constant value of 0.05.
        dev.channels['B'].mode = Mode.SIMV
        dev.channels['B'].constant(0.05)

    # Run the session for at least 10 captured samples in noncontinuous mode.
    for dev, samples in enumerate(session.get_samples(10)):
        print('dev: {}'.format(dev))
        for x in samples:
            print("{: 6f} {: 6f} {: 6f} {: 6f}".format(x[0][0], x[0][1], x[1][0], x[1][1]))
