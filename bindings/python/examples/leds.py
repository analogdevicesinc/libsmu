#!/usr/bin/env python
#
# Iterate through various random RGB LED states for all connected devices.

from signal import signal, SIG_DFL, SIGINT
import sys
import time
from random import randrange

from pysmu import Session, LED


if __name__ == '__main__':
    # don't throw KeyboardInterrupt on Ctrl-C
    signal(SIGINT, SIG_DFL)

    session = Session()

    if not session.devices:
        sys.exit(1)

    while True:
        val = randrange(0, 8)
        for dev in session.devices:
            dev.set_led(LED.red, (val & 0b001))
            dev.set_led(LED.green, (val & 0b010))
            dev.set_led(LED.blue, (val & 0b100))
        time.sleep(.25)
