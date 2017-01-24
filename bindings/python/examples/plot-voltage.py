#!/usr/bin/env python
#
# Simple script showing how to write a sine/cosine wave to the attached devices and
# plot the resulting voltage output for both channels using matplotlib.

from __future__ import print_function

import sys

from pysmu import Session, Mode
import matplotlib.pyplot as plt


if __name__ == '__main__':
    session = Session()

    if not session.devices:
        sys.exit(1)

    for idx, dev in enumerate(session.devices):
        # Output a sine wave for channel A voltage.
        dev.channels['A'].mode = Mode.SVMI
        dev.channels['A'].sine(0, 5, 100, -25)
        # Output a cosine wave for channel B voltage.
        dev.channels['B'].mode = Mode.SVMI
        dev.channels['B'].sine(0, 5, 100, 0)

    # Run the session in noncontinuous mode.
    for i, samples in enumerate(session.get_samples(1001)):
        chan_a = [x[0][0] for x in samples]
        chan_b = [x[1][0] for x in samples]
        plt.figure(i)
        plt.plot(chan_a, label='Channel A')
        plt.plot(chan_b, label='Channel B')
        plt.legend(bbox_to_anchor=(1.05, 1), loc=2, borderaxespad=0.)
        plt.title('Device {}: {}'.format(i, str(session.devices[i])))
        plt.ylabel('Voltage')
        plt.xlabel('Sample number')

    # Show all channel voltage plots for attached devices.
    plt.show()
