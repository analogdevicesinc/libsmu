from __future__ import print_function

import time

try:
    from unittest import mock
except ImportError:
    import mock

import unittest

from pysmu import Session


class TestSession(unittest.TestCase):

    def test_empty(self):
        session = Session()
        self.assertEqual(session.devices, [])

    def test_add_all(self):
        print('ACTION: make sure a device is plugged in')
        session = Session()
        session.add_all()
        self.assertEqual(len(session.devices), 1)

    def test_hotplug(self):
        print('ACTION: plug/unplug a device within 10 seconds')
        session = Session()

        attach = mock.Mock()
        detach = mock.Mock()
        session.hotplug_attach(attach)
        session.hotplug_detach(detach)

        start = time.time()
        while (True):
            time.sleep(1)
            end = time.time()
            elapsed = end - start
            if elapsed > 10 or (attach.called and detach.called):
                break

        self.assertTrue(attach.called)
        self.assertTrue(detach.called)
