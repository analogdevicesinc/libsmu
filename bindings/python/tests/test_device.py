from __future__ import print_function

import os
import tempfile

try:
    from urllib import urlretrieve
except ImportError:
    from urllib.request import urlretrieve

import pytest

from pysmu import Device, Session, DeviceError
from misc import prompt

@pytest.fixture(scope='module')
def session(request):
    session = Session()
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
    pytest.mark.skipif(float(device.fwver) < 2.06)

    default_cal = [
        [0.0, 1.0, 1.0],
        [0.0, 1.0, 1.0],
        [0.0, 1.0, 1.0],
        [0.0, 1.0, 1.0],
        [0.0, 1.0, 1.0],
        [0.0, 1.0, 1.0],
        [0.0, 1.0, 1.0],
        [0.0, 1.0, 1.0],
    ]

    # reset calibration
    device.write_calibration(None)
    assert device.calibration == default_cal

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
    device.write_calibration(cal_data.name)
    new_cal = device.calibration
    assert new_cal != default_cal

    # make sure calibration data survives firmware updates
    fw_url = 'https://github.com/analogdevicesinc/m1k-fw/releases/download/v2.06/m1000.bin'
    # fetch old/new firmware files from github
    fw = tempfile.NamedTemporaryFile()
    urlretrieve(fw_url, fw.name)
    session.add_all()
    session.flash_firmware(fw.name)
    prompt('unplug/replug the device')
    session.scan()
    session.add_all()
    device = session.devices[0]
    assert new_cal == device.calibration

    # writing good calibration file
    device.write_calibration(default_cal_data)
    assert device.calibration == default_cal

def test_samba_mode(session, device):
    # assumes an internet connection is available and github is up
    fw_url = 'https://github.com/analogdevicesinc/m1k-fw/releases/download/v2.06/m1000.bin'

    # fetch old/new firmware files from github
    fw = tempfile.NamedTemporaryFile()
    urlretrieve(fw_url, fw.name)

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
    session.flash_firmware(fw.name)
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
