from __future__ import print_function

import sys
import time

try:
    from unittest import mock
except ImportError:
    import mock

# input = raw_input in py3, copy this for py2
if sys.hexversion < 0x03000000:
    input = raw_input

import unittest

from pysmu import Session


def prompt(s):
    """Prompt the user to verify test setup before continuing."""
    input('ACTION: {} (hit Enter to continue)'.format(s))


class TestSession(unittest.TestCase):

    def test_empty(self):
        session = Session()
        self.assertEqual(session.devices, [])

    def test_scan(self):
        prompt('make sure at least one device is plugged in')
        session = Session()
        session.scan()

        # available devices haven't been added to the session yet
        self.assertTrue(len(session.available_devices) > 0)
        self.assertNotEqual(len(session.available_devices), len(session.devices))

    def test_add(self):
        session = Session()
        self.assertEqual(len(session.devices), 0)

        session.scan()
        self.assertTrue(len(session.available_devices) > 0)
        session.add(session.available_devices[0])
        self.assertEqual(len(session.devices), 1)
        self.assertEqual(session.devices[0], session.available_devices[0])

    def test_remove(self):
        session = Session()
        session.add_all()
        self.assertTrue(len(session.devices) > 0)
        self.assertEqual(len(session.available_devices), len(session.devices))
        session.remove(session.devices[0])
        self.assertNotEqual(len(session.available_devices), len(session.devices))

    def test_add_all(self):
        session = Session()
        self.assertEqual(len(session.devices), 0)
        session.add_all()

        # all available devices should be in the session
        self.assertTrue(len(session.devices) > 0)
        self.assertEqual(len(session.available_devices), len(session.devices))

    def test_hotplug(self):
        prompt('unplug/plug a device within 10 seconds')
        session = Session()
        session.add_all()

        attach = mock.Mock()
        detach = mock.Mock()
        session.hotplug_attach(attach)
        session.hotplug_detach(detach)

        start = time.time()
        print('waiting hotplug events...')
        while (True):
            time.sleep(1)
            end = time.time()
            elapsed = end - start
            if elapsed > 10 or (attach.called and detach.called):
                break

        self.assertTrue(attach.called)
        self.assertTrue(detach.called)

    def test_hotplug_add_remove(self):
        prompt('make sure all devices are removed, then unplug/plug a device within 10 seconds')
        session = Session()

        def attach(dev):
            serial = dev.serial
            session.add(dev)
            attached = any(d.serial == serial for d in session.devices)
            self.assertTrue(attached)

        def detach(dev):
            serial = dev.serial
            session.remove(dev)
            detached = any(d.serial != serial for d in session.devices)
            self.assertTrue(detached)

        session.hotplug_attach(attach)
        session.hotplug_detach(detach)

        start = time.time()
        print('waiting hotplug events...')
        while (True):
            time.sleep(1)
            end = time.time()
            elapsed = end - start
            if elapsed > 10 or (attach.called and detach.called):
                break

        self.assertTrue(attach.called)
        self.assertTrue(detach.called)
