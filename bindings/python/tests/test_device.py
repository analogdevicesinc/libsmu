from __future__ import print_function

import filecmp
import os
import tempfile

import pytest

from pysmu import Device, Session, DeviceError
from misc import prompt, NEW_FW_URL, OLD_FW_URL, OLD_FW, NEW_FW

@pytest.fixture(scope='module')
def session(request):
    session = Session(add_all=False)
    session.scan()
    return session

@pytest.fixture(scope='module')
def device(session):
    return session.available_devices[0]

def test_device_serial(device):
    prompt('make sure at least one device is plugged in')
    assert device.serial != ""

def test_device_fwver(device):
    assert device.fwver != ""

def test_device_hwver(device):
    assert device.hwver != ""

def test_calibration(device):
    assert len(device.calibration) == 8

def test_write_calibration(session, device):
    default_cal = [[0.0, 1.0, 1.0] for x in range(8)]

    # old firmware versions don't support writing calibration data
    session.add_all()
    session.flash_firmware(OLD_FW)
    prompt('unplug/replug the device')
    session.scan()
    session.add_all()
    device = session.devices[0]
    assert float(device.fwver) < 2.06
    assert device.calibration == default_cal
    with pytest.raises(DeviceError):
        device.write_calibration(None)

    # update to firmware supporting calibration
    session.flash_firmware(NEW_FW)
    prompt('unplug/replug the device')
    session.scan()
    session.add_all()
    device = session.devices[0]

    # writing nonexistent calibration file
    with pytest.raises(DeviceError):
        device.write_calibration('nonexistent')

    # writing bad calibration file
    cal_data = tempfile.NamedTemporaryFile()
    with open(cal_data.name, 'w') as f:
        f.write('foo')
    with pytest.raises(DeviceError):
        device.write_calibration(cal_data.name)

    # find default calibration file in repo
    dn = os.path.dirname
    default_cal_data = os.path.join(dn(dn(dn(dn(__file__)))), 'contrib', 'calib.txt')

    # writing modified calibration file
    cal_data = tempfile.NamedTemporaryFile()
    with open(default_cal_data) as default, open(cal_data.name, 'w') as f:
        for line in default:
            # randomly munge cal data
            if line.strip() == "<0.0000, 0.0000>":
                f.write("<1.0000, 2.0000>\n")
            else:
                f.write(line)
    # verify that the new file differs from the default
    assert not filecmp.cmp(cal_data.name, default_cal_data)
    device.write_calibration(cal_data.name)
    new_cal = device.calibration
    assert new_cal != default_cal

    # make sure calibration data survives firmware updates
    session.flash_firmware(NEW_FW)
    prompt('unplug/replug the device')
    session.scan()
    session.add_all()
    device = session.devices[0]
    assert new_cal == device.calibration

    # reset calibration
    device.write_calibration(None)
    assert device.calibration == default_cal

    # writing good calibration file
    device.write_calibration(default_cal_data)
    assert device.calibration == default_cal

def test_samba_mode(session, device):
    # supported devices exist in the session
    num_available_devices = len(session.available_devices)
    assert num_available_devices
    orig_serial = session.available_devices[0].serial

    # pushing one into SAM-BA mode drops it from the session after rescan
    device.samba_mode()
    session.scan()
    assert len(session.available_devices) == num_available_devices - 1
    assert not any(d.serial == orig_serial for d in session.available_devices)

    # flash device in SAM-BA mode
    session.flash_firmware(NEW_FW)
    prompt('unplug/replug the device')
    session.scan()

    # device is re-added after hotplug
    assert len(session.available_devices) == num_available_devices
    dev = session.available_devices[0]
    assert dev.serial == orig_serial
    assert dev.fwver == '2.06'

def test_ctrl_transfer(device):
    # set pin input and get pin value
    val = device.ctrl_transfer(0xc0, 0x91, 0, 0, 0, 1, 100)
    assert val == [1]
    val = device.ctrl_transfer(0xc0, 0x91, 2, 0, 0, 1, 100)
    assert val == [0]
