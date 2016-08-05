from __future__ import print_function

import errno
import sys
import tempfile
import time
import unittest
from urllib import urlretrieve

try:
    from unittest import mock
except ImportError:
    import mock

from pysmu import Session, SessionError
from .misc import prompt

# XXX: Hack to run tests in defined class order, required due to assumptions on
# when a device is physically plugged in since we don't want to prompt at the
# beginning of every function.
ln = lambda f: getattr(TestSession, f).im_func.func_code.co_firstlineno
lncmp = lambda _, a, b: cmp(ln(a), ln(b))
unittest.TestLoader.sortTestMethodsUsing = lncmp


class TestSession(unittest.TestCase):

    def setUp(self):
        self.session = Session()

    def tearDown(self):
        del self.session

    def test_empty(self):
        self.assertEqual(self.session.devices, [])

    def test_scan(self):
        prompt('make sure at least one device is plugged in')
        self.session.scan()

        # available devices haven't been added to the session yet
        self.assertTrue(self.session.available_devices)
        self.assertNotEqual(len(self.session.available_devices), len(self.session.devices))

    def test_add(self):
        self.assertFalse(self.session.devices)

        self.session.scan()
        self.assertTrue(self.session.available_devices)
        dev = self.session.available_devices[0]
        self.session.add(dev)
        self.assertEqual(len(self.session.devices), 1)
        self.assertEqual(self.session.devices[0].serial, dev.serial)

        # re-adding the same device does nothing
        self.session.add(dev)
        self.assertEqual(len(self.session.devices), 1)
        self.assertEqual(self.session.devices[0].serial, dev.serial)

    def test_remove(self):
        self.session.add_all()
        self.assertTrue(self.session.devices)
        self.assertEqual(len(self.session.available_devices), len(self.session.devices))
        dev = self.session.devices[0]
        self.session.remove(dev)
        self.assertFalse(any(d.serial == dev.serial for d in self.session.devices))
        self.assertNotEqual(len(self.session.available_devices), len(self.session.devices))

        # removing already removed devices fails
        with self.assertRaises(SessionError) as cm:
            self.session.remove(dev)
        e = cm.exception
        self.assertEqual(e.errcode, errno.ENXIO)

    def test_add_all(self):
        self.assertFalse(self.session.devices)
        self.session.add_all()

        # all available devices should be in the session
        self.assertTrue(self.session.devices)
        self.assertEqual(len(self.session.available_devices), len(self.session.devices))

    def test_destroy(self):
        self.session.scan()
        # available devices haven't been added to the session yet
        self.assertTrue(self.session.available_devices)
        serial = self.session.available_devices[0].serial
        self.session.destroy(self.session.available_devices[0])
        self.assertFalse(any(d.serial == serial for d in self.session.available_devices))

    def test_flash_firmware(self):
        # assumes an internet connection is available and github is up
        old_fw_url = 'https://github.com/analogdevicesinc/m1k-fw/releases/download/v2.02/m1000.bin'
        new_fw_url = 'https://github.com/analogdevicesinc/m1k-fw/releases/download/v2.06/m1000.bin'

        # fetch old/new firmware files from github
        old_fw = tempfile.NamedTemporaryFile()
        new_fw = tempfile.NamedTemporaryFile()
        urlretrieve(old_fw_url, old_fw.name)
        urlretrieve(new_fw_url, new_fw.name)

        self.session.add_all()
        self.assertEqual(len(self.session.devices), 1)
        serial = self.session.devices[0].serial

        # flash old firmware
        self.session.flash_firmware(old_fw.name)
        prompt('unplug/replug the device')
        self.session.add_all()
        self.assertEqual(len(self.session.devices), 1)
        self.assertEqual(self.session.devices[0].serial, serial)
        self.assertEqual(self.session.devices[0].fwver, '2.02')

        # flash new firmware
        self.session.flash_firmware(new_fw.name)
        prompt('unplug/replug the device')
        self.session.add_all()
        self.assertEqual(len(self.session.devices), 1)
        self.assertEqual(self.session.devices[0].serial, serial)
        self.assertEqual(self.session.devices[0].fwver, '2.06')

    def test_hotplug(self):
        prompt('unplug/plug a device within 10 seconds')
        self.session.add_all()

        # create fake attach/detach callbacks to check basic triggering
        fake_attach = mock.Mock()
        fake_detach = mock.Mock()
        self.session.hotplug_attach(fake_attach)
        self.session.hotplug_detach(fake_detach)

        # create more realistic callbacks that try adding/removing the
        # hotplugged device from a session
        def attach(dev):
            serial = dev.serial
            self.session.add(dev)
            self.assertTrue(any(d.serial == serial for d in self.session.devices))

        def detach(dev):
            serial = dev.serial
            self.session.remove(dev, detached=True)
            self.assertFalse(any(d.serial == serial for d in self.session.devices))

        self.session.hotplug_attach(attach)
        self.session.hotplug_detach(detach)

        start = time.time()
        print('waiting hotplug events...')
        while (True):
            time.sleep(1)
            end = time.time()
            elapsed = end - start
            if elapsed > 10 or (fake_attach.called and fake_detach.called):
                break

        self.assertTrue(fake_attach.called)
        self.assertTrue(fake_detach.called)
