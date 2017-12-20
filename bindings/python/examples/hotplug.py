#!/usr/bin/env python
#
# Simple script showing how custom functions can be executed during hotplug
# events, use Ctrl-C to exit.

from __future__ import print_function

from signal import signal, SIG_DFL, SIGINT
import time

from pysmu import Session


def attached(dev):
    """Run when a device is plugged in."""
    print('device attached: {}'.format(dev))

def detached(dev):
    """Run when a device is unplugged."""
    print('device detached: {}'.format(dev))


if __name__ == '__main__':
    # don't throw KeyboardInterrupt on Ctrl-C
    signal(SIGINT, SIG_DFL)

    session = Session()

    # Register the functions above to get triggered during physical attach or
    # detach events. Multiple functions can be registered for either trigger if
    # required.
    session.hotplug_attach(attached)
    session.hotplug_detach(detached)

    print('waiting for hotplug events...')
    while True:
        time.sleep(1)
