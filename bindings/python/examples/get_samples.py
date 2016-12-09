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
        sys.exit()

    for dev in session.devices:
        # Set all devices in the session to use source voltage, measure current
        # mode for channel A with a constant value of 4.
        dev.channels['A'].mode = Mode.SVMI
        dev.channels['A'].constant(4)
        # Set all devices in the session to use source current, measure voltage
        # mode for channel B with a constant value of -0.2.
        dev.channels['B'].mode = Mode.SIMV
        dev.channels['B'].constant(-0.2)

    # Run the session for 10 incoming samples in noncontinuous mode. This means
    # that after the requested number of samples is collected, all the devices
    # in the session are turned off and reset into HI-Z mode.
    session.run(10)

    for idx, dev in enumerate(session.devices):
        print('dev: {}'.format(idx + 1))
        for x in dev.get_samples(10):
            print("{:6f} {:6f} {:6f} {:6f}".format(x[0][0], x[0][1], x[1][0], x[1][1]))
