#!/usr/bin/env python
#
# Simple script showing how to write a sine/cosine wave to the attached devices and
# plot the resulting voltage output for both channels using matplotlib.

from __future__ import print_function

from collections import defaultdict
import sys

from pysmu import Session, Mode
import matplotlib.pyplot as plt


if __name__ == '__main__':
    session = Session()

    if not session.devices:
        sys.exit(1)

    waveform_samples = 500

    for i, dev in enumerate(session.devices):
        # Output a sine wave for channel A voltage.
        dev.channels['A'].mode = Mode.SVMI
        dev.channels['A'].sine(0, 5, waveform_samples, -(waveform_samples / 4))
        # Output a cosine wave for channel B voltage.
        dev.channels['B'].mode = Mode.SVMI
        dev.channels['B'].sine(0, 5, waveform_samples, 0)

    chan_a = defaultdict(list)
    chan_b = defaultdict(list)

    # Run the session in noncontinuous mode.
    for _x in range(10):
        for i, samples in enumerate(session.get_samples(waveform_samples / 5)):
            chan_a[i].extend([x[0][0] for x in samples])
            chan_b[i].extend([x[1][0] for x in samples])

    for i, dev in enumerate(session.devices):
        plt.figure(i)
        plt.plot(chan_a[i], label='Channel A')
        plt.plot(chan_b[i], label='Channel B')
        plt.legend(bbox_to_anchor=(1.05, 1), loc=2, borderaxespad=0.)
        plt.title('Device {}: {}'.format(i, str(session.devices[i])))
        plt.ylabel('Voltage')
        plt.xlabel('Sample number')

    # Show all channel voltage plots for attached devices.
    plt.show()
