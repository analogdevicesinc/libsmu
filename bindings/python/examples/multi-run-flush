#!/usr/bin/env python
#
# Simple script showing how to perform a non-continuous, multi-run session
# while changing cyclic buffer values.

from __future__ import print_function

import sys

from pysmu import Session, Mode


if __name__ == '__main__':
    session = Session()

    if not session.devices:
        sys.exit()

    for x in range(10):
        session.flush()

        for dev in session.devices:
            v = x % 6
            # modes have to be reset on each loop since they're currently reset
            # to HI_Z at the end of each run.
            dev.channels['A'].mode = Mode.SVMI
            dev.channels['A'].constant(v)
            dev.channels['B'].mode = Mode.SIMV
            dev.channels['B'].constant(-.2)

        session.run(10)

        for idx, dev in enumerate(session.devices):
            print('dev: {}: chan A voltage: {}'.format(idx + 1, v))
            for x in dev.get_samples(10):
                print("{:6f} {:6f} {:6f} {:6f}".format(x[0][0], x[0][1], x[1][0], x[1][1]))
