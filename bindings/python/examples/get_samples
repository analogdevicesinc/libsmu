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
        dev.channels['A'].mode = Mode.SVMI
        dev.channels['A'].constant(4)
        dev.channels['B'].mode = Mode.SIMV
        dev.channels['B'].constant(-.2)

    session.run(10)

    for idx, dev in enumerate(session.devices):
        print('dev: {}'.format(idx + 1))
        for x in dev.get_samples(10):
            print("{:6f} {:6f} {:6f} {:6f}".format(x[0][0], x[0][1], x[1][0], x[1][1]))
