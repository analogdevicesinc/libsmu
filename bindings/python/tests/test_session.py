from __future__ import print_function

import errno
import time

try:
    from unittest import mock
except ImportError:
    import mock

import pytest

from pysmu import Session, SessionError
from misc import prompt, OLD_FW_URL, NEW_FW_URL, OLD_FW, NEW_FW


@pytest.yield_fixture(scope='function')
def session():
    s = Session(add_all=False)
    yield s

    # force session destruction
    s._close()

def test_empty(session):
    assert len(session.devices) == 0

def test_scan(session):
    prompt('make sure at least one device is plugged in')
    session.scan()

    # available devices haven't been added to the session yet
    assert session.available_devices
    assert len(session.available_devices) != len(session.devices)

def test_add(session):
    assert not session.devices

    session.scan()
    assert session.available_devices
    dev = session.available_devices[0]
    session.add(dev)
    assert len(session.devices) == 1
    assert session.devices[0].serial == dev.serial

    # re-adding the same device does nothing
    session.add(dev)
    assert len(session.devices) == 1
    assert session.devices[0].serial == dev.serial

def test_add_all(session):
    assert not session.devices
    session.add_all()

    # all available devices should be in the session
    assert session.devices
    assert len(session.available_devices) == len(session.devices)

def test_remove(session):
    session.add_all()
    assert session.devices
    assert len(session.available_devices) == len(session.devices)
    dev = session.devices[0]
    session.remove(dev)
    assert not any(d.serial == dev.serial for d in session.devices)
    assert len(session.available_devices) != len(session.devices)

    # removing already removed devices fails
    with pytest.raises(SessionError) as excinfo:
        session.remove(dev)
    assert excinfo.value.errcode == errno.ENXIO

def test_destroy(session):
    session.scan()
    # available devices haven't been added to the session yet
    assert session.available_devices
    serial = session.available_devices[0].serial
    session.destroy(session.available_devices[0])
    assert not any(d.serial == serial for d in session.available_devices)

def test_flash_firmware(session):
    session.add_all()
    assert len(session.devices) == 1
    serial = session.devices[0].serial

    # flash old firmware
    print('flashing firmware 2.02...')
    session.flash_firmware(OLD_FW)
    prompt('unplug/replug the device')
    session.add_all()
    assert len(session.devices) == 1
    assert session.devices[0].serial == serial
    assert session.devices[0].fwver == '2.02'

    # flash new firmware
    print('flashing firmware 2.06...')
    session.flash_firmware(NEW_FW)
    prompt('unplug/replug the device')
    session.add_all()
    assert len(session.devices) == 1
    assert session.devices[0].serial == serial
    assert session.devices[0].fwver == '2.06'

def test_hotplug(session):
    prompt('unplug/plug a device within 10 seconds')
    session.add_all()

    # create fake attach/detach callbacks to check basic triggering
    fake_attach = mock.Mock()
    fake_detach = mock.Mock()
    session.hotplug_attach(fake_attach)
    session.hotplug_detach(fake_detach)

    # create more realistic callbacks that try adding/removing the
    # hotplugged device from a session
    def attach(dev):
        serial = dev.serial
        session.add(dev)
        assert any(d.serial == serial for d in session.devices)

    def detach(dev):
        serial = dev.serial
        session.remove(dev, detached=True)
        assert not any(d.serial == serial for d in session.devices)

    session.hotplug_attach(attach)
    session.hotplug_detach(detach)

    start = time.time()
    print('waiting hotplug events...')
    while (True):
        time.sleep(1)
        end = time.time()
        elapsed = end - start
        if elapsed > 10 or (fake_attach.called and fake_detach.called):
            break

    assert fake_attach.called, 'attach callback not called'
    assert fake_detach.called, 'detach callback not called'
