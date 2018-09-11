# distutils: language = c++

from __future__ import absolute_import, print_function, division

from collections import OrderedDict
import os
from signal import signal, SIG_DFL, SIGINT
import warnings

from libc.stdint cimport uint32_t, uint64_t
from libcpp.vector cimport vector

# enum is only in py34 and up, use vendored backport if the system doesn't have
# it available for py27
try:
    from enum import Enum
except ImportError:
    from ._vendor.enum import Enum

from . cimport cpp_libsmu
from .array cimport array
from .exceptions import *
from .utils import iterify

__version__ = cpp_libsmu.libsmu_version_str().decode()


cdef extern from "Python.h" nogil:
    void PyEval_InitThreads()


# Workaround only py34 and up having native enum support; switch to cython
# import methods once sourcing the C++ definition directly is supported.
class Mode(Enum):
    """Available modes for channels."""
    HI_Z = 0 # floating
    SVMI = 1 # source voltage, measure current
    SIMV = 2 # source current, measure voltage
    HI_Z_SPLIT = 3
    SVMI_SPLIT = 4
    SIMV_SPLIT = 5


class LED(Enum):
    """Available device LEDs to control."""
    red = 47
    green = 29
    blue = 28
    all = 0


cdef class Session:
    # pointer to the underlying C++ smu::Session object
    cdef cpp_libsmu.Session *_session
    cdef public bint ignore_dataflow

    def __cinit__(self):
        self._session = new cpp_libsmu.Session()

        if self._session is NULL:
            raise MemoryError()

        # initialize/acquire the GIL
        PyEval_InitThreads()

    def __init__(self, add_all=True, ignore_dataflow=True, sample_rate=0, queue_size=None):
        """Initialize a session.

        Attributes:
            add_all (bool, optional): Add all attached devices to the session on initialization.
            ignore_dataflow (bool, optional): Ignore sample drops or write timeouts for all
                devices in the session which mainly affects running in continuous mode.
            sample_rate (int, optional): Sample rate to run the session at.
                A sample rate of 0 (the default) causes the session to use the
                default sample rate.
            queue_size (int, optional): Size of input/output sample queues for
                every device (defaults to 10000).
        """
        if queue_size is not None:
            self.queue_size = queue_size

        if add_all:
            self.add_all()

        self.ignore_dataflow = ignore_dataflow
        self.configure(sample_rate)

        signal(SIGINT, self._signal_hander)

    def _signal_hander(self, signum, frame):
        self._close()
        # resend the default SIGINT signal after closing the session
        signal(SIGINT, SIG_DFL)
        os.kill(os.getpid(), SIGINT)

    def hotplug_attach(self, *funcs):
        """Register a function to run on a device attach event.

        Attributes:
            funcs: Python function(s) to run on device attach events. The
                function should accept a single parameter, the device object
                being attached.
        """
        for f in funcs:
            self._session.hotplug_attach(self._hotplug_callback, <void*>f)

    def hotplug_detach(self, *funcs):
        """Register a function to run on a device detach event.

        Attributes:
            funcs: Python function(s) to run on device detach events. The
                function should accept a single parameter, the device object
                being detached.
        """
        for f in funcs:
            self._session.hotplug_detach(self._hotplug_callback, <void*>f)

    @staticmethod
    cdef void _hotplug_callback(cpp_libsmu.Device *device, void *func) with gil:
        """Internal proxy to run the python hotplug functions from the C++ side."""
        dev = Device._create(device)
        (<object>func)(dev)

    property available_devices:
        """Devices that are accessible on the system."""
        def __get__(self):
            return tuple(Device._create(d) for d in self._session.m_available_devices)

    property devices:
        """Devices that are included in this session."""
        def __get__(self):
            devs = []
            index = 0
            for d in self._session.m_devices:
                devs.append(SessionDevice._session_create(
                    d, session=self, session_index=index, ignore_dataflow=self.ignore_dataflow))
                index += 1
            return tuple(devs)

    property active_devices:
        """Number of devices that are currently active (streaming data) in this session."""
        def __get__(self):
            return self._session.m_active_devices

    property queue_size:
        """Input/output sample queue size."""
        def __get__(self):
            return self._session.m_queue_size
        def __set__(self, size):
            self._session.m_queue_size = size

    property cancelled:
        """Cancellation status of a session."""
        def __get__(self):
            return self._session.cancelled()

    property continuous:
        """Continuous status of a session."""
        def __get__(self):
            return bool(self._session.m_continuous)

    property sample_rate:
        """Session sample rate."""
        def __get__(self):
            return self._session.m_sample_rate
        def __set__(self, rate):
            self.configure(rate)

    def scan(self):
        """Scan the system for supported devices.

        Raises: SessionError on failure.
        Returns: The number of devices found is returned.
        """
        cdef int ret = 0
        ret = self._session.scan()
        if ret < 0:
            raise SessionError('failed scanning for supported devices', ret)

        return ret

    def _check_fw_versions(self):
        """Check session for devices with different firmware versions."""
        if len(self.devices) > 1:
            fw_versions = [dev.fwver for dev in self.devices]
            if len(set(fw_versions)) > 1:
                warnings.warn(
                    "differing firmware versions in session devices: {}".format(
                        ', '.join(fw_versions)), RuntimeWarning)

    def add_all(self):
        """Scan the system and add all supported devices to the session.

        Raises: SessionError on failure.
        Returns: The number of devices added to the session is returned.
        """
        cdef int ret = 0
        ret = self._session.add_all()
        if __debug__:
            self._check_fw_versions()

        if ret < 0:
            raise SessionError('failed scanning and/or adding all supported devices', ret)

        return ret

    def add(self, Device dev):
        """Add a device to the session.

        Raises: SessionError on failure.
        """
        cdef int ret = 0
        ret = self._session.add(dev._device)
        if __debug__:
            self._check_fw_versions()

        if ret:
            raise SessionError('failed adding device', ret)

    def remove(self, Device dev, bint detached=False):
        """Remove a device from the session."""
        cdef int ret = 0
        ret = self._session.remove(dev._device, detached)
        if ret:
            raise SessionError('failed removing device', ret)

    def destroy(self, Device dev):
        """Drop a device from the list of available devices."""
        cdef int ret = 0
        ret = self._session.destroy(dev._device)
        if ret:
            raise SessionError('failed destroying device', ret)

    def configure(self, uint32_t sample_rate=0):
        """Configure the session's sample rate.

        Attributes:
            sample_rate (int, optional): Sample rate to run the session at.
                A sample rate of 0 (the default) causes the session to use the
                default sample rate.

        Raises: SessionError on failure.
        """
        if sample_rate < 0:
            raise ValueError('invalid sample rate: {}'.format(sample_rate))

        cdef int ret = 0
        ret = self._session.configure(sample_rate)
        if ret < 0:
            raise SessionError('failed configuring device', ret)

    def run(self, int samples):
        """Run the configured capture for a certain number of samples.

        Attributes:
            samples (int): Number of samples to run the session for.
                If 0, run in continuous mode.
        """
        if samples < 0:
            raise ValueError('invalid number of samples: {}'.format(samples))

        cdef int ret = 0

        try:
            ret = self._session.run(samples)
        except RuntimeError as e:
            write_timeout_err = 'data write timeout'
            sample_drop_err = 'data sample dropped'
            if e.message[:len(sample_drop_err)] == sample_drop_err:
                if not self.ignore_dataflow:
                    raise SampleDrop(e.message)
            elif e.message[:len(write_timeout_err)] == write_timeout_err:
                raise WriteTimeout(e.message)
            else:
                raise SessionError(str(e))

        if ret:
            raise SessionError('failed running session stream', ret)

    def start(self, int samples):
        """Start the currently configured capture, but do not wait for it to complete.

        Attributes:
            samples (int): Number of samples to capture before stopping.
                If 0, run in continuous mode.
        """
        if samples < 0:
            raise ValueError('invalid number of samples: {}'.format(samples))

        cdef int ret = 0
        ret = self._session.start(samples)
        if ret:
            raise SessionError('failed starting session stream', ret)

    def cancel(self):
        """Cancel the current capture and block while waiting for completion.

        Raises: SessionError on failure.
        """
        cdef int ret = 0
        ret = self._session.cancel()
        if ret:
            raise SessionError('failed canceling device transfers', ret)

    def flush(self):
        """Flush the read and write queues for all devices in a session."""
        self._session.flush()

    def end(self):
        """Block until all devices have completed, then turn off the devices."""
        cdef int ret = 0
        ret = self._session.end()
        if ret:
            raise SessionError('failed ending session stream', ret)

    def flash_firmware(self, path, devices=()):
        """Update firmware for a given device.

        Attributes:
            path (str): Path to firmware file.
            devices (Device or iterable of `Device`s, optional): The device(s) targeted for
                updating. If not provided or empty, all supported devices on the system
                will be flashed.

        >>> from pysmu import Session
        >>> session = Session()
        >>> session.flash_firmware('/path/to/m1000-2.06.bin')
        >>> for dev in session.devices:
        >>>     assert dev.fwver == '2.06'

        Raises: SessionError on writing failures.
        """
        cdef vector[cpp_libsmu.Device*] devs
        for d in iterify(devices):
            devs.push_back((<Device>d)._device)

        try:
            self._session.flash_firmware(path.encode(), devs)
        except RuntimeError as e:
            raise SessionError(str(e))

    def read(self, size_t num_samples, int timeout=0,skipsamples=False):
        """Acquire all signal samples from all device in the session.

        Args:
            num_samples (int): number of samples to read
            timeout (int, optional): amount of time in milliseconds to wait for samples to be available.
                - If 0 (the default), return immediately.
                - If -1, block indefinitely until the requested number of samples is returned.

        Raises: DeviceError on reading failures.
        Returns: A list of lists containing sample values.
        """
        samples = []
        for dev in self.devices:
            samples.append(dev.read(num_samples, timeout=timeout,skipsamples=skipsamples))
        return samples

    def write(self, data, bint cyclic=False):
        """Write data to all devices in a session.

        Args:
            data: iterable of sample values for each channel of each device in the session
                Example data for session with two devices: [[[1][3]],[[],[4]]]
            cyclic (bool, optional): continuously iterate over the same buffer

        Raises: DeviceError on writing failures.
        """
        try:
            for i, chan_data in enumerate(data):
                self.devices[i].write(chan_data[0], channel=0, cyclic=cyclic)
                self.devices[i].write(chan_data[1], channel=1, cyclic=cyclic)
        except IndexError:
            raise ValueError("invalid data buffer format")

    def get_samples(self, num_samples):
        """Acquire signal samples from all devices in a session.

        This runs the devices in a noncontinuous fashion and blocks until the
        requested number of samples have been captured.

        Args:
            num_samples (int): number of samples to read

        Raises: DeviceError on reading failures.
        Returns: A list of lists containing the specified number of sample values for
            all devices in the session.
        """
        data = [[] for dev in self.devices]

        # maximum number of samples that can fit in the internal, incoming queue
        max_samples = self.queue_size
        required_samples = num_samples

        # If requested samples are bigger than internal queue size, then
        # multiple run/read calls must be used.
        while required_samples:
            run_samples = required_samples if required_samples < max_samples else max_samples
            self.run(run_samples)
            for i, x in enumerate(self.read(run_samples, -1)):
                data[i].extend(x)
            required_samples -= run_samples

        return data

    def _close(self):
        """Force session destruction."""
        # TODO: There probably is some way of properly using shared/weak ptrs for
        # the relevant underlying C++ objects so calling del on a session object calls __dealloc__.
        del self._session
        self._session = NULL

    def __dealloc__(self):
        """Destroy session if it exists."""
        if self._session is not NULL:
            self._close()


cdef class Device:
    # pointer to the underlying C++ smu::Device object
    cdef cpp_libsmu.Device *_device

    @staticmethod
    cdef _create(cpp_libsmu.Device *device) with gil:
        """Internal method to wrap C++ smu::Device objects."""
        d = Device()
        d._device = device
        return d

    property serial:
        """Return device's serial number."""
        def __get__(self):
            return self._device.m_serial.decode()
        def __set__(self, serial):
            cdef int ret = 0
            ret = self._device.set_serial(serial)
            if ret < 0:
                raise DeviceError('failed setting custom serial number')

    property fwver:
        """Return device's firmware revision."""
        def __get__(self):
            return self._device.m_fwver.decode()

    property hwver:
        """Return device's hardware revision."""
        def __get__(self):
            return self._device.m_hwver.decode()

    property calibration:
        """Read calibration data from the device's EEPROM."""
        def __get__(self):
            cdef vector[vector[float]] cal
            self._device.calibration(&cal)
            return cal;

    property default_rate:
        """Get the default sample rate for the device."""
        def __get__(self):
            return self._device.get_default_rate()

    def write_calibration(self, file):
        """Write calibration data to the device's EEPROM.

        Args:
            file (str): path to calibration file
                (use None to reset the calibration to the defaults)

        Raises: DeviceError on writing failures.
        """
        cdef int ret = 0
        cdef const char* cal_path
        if file is None:
            cal_path = NULL
        else:
            file = file.encode()
            cal_path = file

        ret = self._device.write_calibration(cal_path)
        if ret < 0:
            raise DeviceError('failed writing device calibration data')

    def __str__(self):
        return 'serial {} : fw {} : hw {}'.format(self.serial, self.fwver, self.hwver)

    def samba_mode(self):
        """Enable SAM-BA bootloader mode on the device."""
        cdef int ret = 0
        ret = self._device.samba_mode()
        if ret:
            raise DeviceError('failed to enable SAM-BA mode', ret)

    def set_led(self, leds):
        """Set device LEDs.

        Args:
            leds: an integer number, the bits of the number represents the states of the leds (1-on 0-off) in order (RGB or DS3,DS2,DS1 on rev F) 
        Raises: IOError on USB failures.
        """
        self._device.set_led(leds)

    def ctrl_transfer(self, bm_request_type, b_request, wValue, wIndex,
                      data, wLength, timeout):
        """Perform raw USB control transfers.

        The arguments map directly to those of the underlying
        libusb_control_transfer call.

        Args:
            bm_request_type: the request type field for the setup packet
            b_request: the request field for the setup packet
            wValue: the value field for the setup packet
            wIndex: the index field for the setup packet
            data: a suitably-sized data buffer for either input or output
            wLength: the length field for the setup packet
            timeout: timeout (in milliseconds) that this function should wait
                before giving up due to no response being received

        Returns: the number of bytes actually transferred
        Raises: IOError on USB failures
        """
        cdef int ret = 0

        data = str(data).encode()

        if bm_request_type & 0x80 == 0x80:
            if data == '0':
                data = '\x00' * wLength
        else:
            wLength = 0

        ret = self._device.ctrl_transfer(bm_request_type, b_request, wValue,
                                         wIndex, data, wLength, timeout)
        if ret < 0:
            raise IOError(abs(ret), 'USB control transfer failed')
        else:
            if bm_request_type & 0x80 == 0x80:
                return map(ord, data)
            else:
                return ret


cdef class SessionDevice(Device):
    # session the device belongs to
    cdef Session _session
    cdef unsigned _session_index
    cdef public bint ignore_dataflow
    cdef readonly object channels

    def __init__(self, Session session, unsigned session_index, bint ignore_dataflow=False):
        """Initialize a device.

        Attributes:
            session (Session object): Session that the device belongs to.
            session_index (unsigned): Device index in the session it belongs to.
            ignore_dataflow (bool): Ignore sample drops or write timeouts for the device.
        """
        self.ignore_dataflow = ignore_dataflow
        self._session = session
        self._session_index = session_index
        self.channels = OrderedDict([
            ('A', Channel(self, 0)),
            ('B', Channel(self, 1)),
        ])

    @staticmethod
    cdef _session_create(cpp_libsmu.Device *device, Session session, unsigned session_index, bint ignore_dataflow=False) with gil:
        """Internal method to wrap C++ smu::Device objects."""
        d = SessionDevice(session=session, session_index=session_index, ignore_dataflow=ignore_dataflow)
        d._device = device
        return d

    property sample_rate:
        """The device's configured sample rate."""
        def __get__(self):
            return self._session.sample_rate

    property overcurrent:
        """Return the overcurrent status related to the most recent data acquisition."""
        def __get__(self):
            return self._device.m_overcurrent

    def read(self, size_t num_samples, int timeout=0,bint skipsamples = False):
        """Acquire all signal samples from a device.

        Args:
            num_samples (int): number of samples to read
            timeout (int, optional): amount of time in milliseconds to wait for samples to be available.
                - If 0 (the default), return immediately.
                - If -1, block indefinitely until the requested number of samples is returned.

        Raises: DeviceError on reading failures.
        Returns: A list containing sample values.
        """
        cdef ssize_t ret = 0
        cdef vector[array[float, cpp_libsmu.four]] buf

        try:
            ret = self._device.read(buf, num_samples, timeout,skipsamples)
        except SystemError as e:
            raise DeviceError(str(e))
        except RuntimeError as e:
            err = 'data sample dropped'
            if e.message[:len(err)] == err:
                if not self.ignore_dataflow:
                    raise SampleDrop(e.message)
            else:
                raise DeviceError(str(e))

        if ret < 0:
            raise DeviceError('failed reading from device', ret)

        return [((x[0], x[1]), (x[2], x[3])) for x in buf]

    def write(self, data, unsigned channel, bint cyclic=False):
        """Write data to a specified channel of the device.

        Args:
            data: iterable of sample values
            channel (0 or 1): channel to write samples to
            cyclic (bool, optional): continuously iterate over the same buffer

        Raises: DeviceError on writing failures.
        """
        # don't waste time writing empty data sets
        if len(data) == 0:
            return

        cdef int ret = 0
        cdef vector[float] buf = data

        try:
            ret = self._device.write(buf, channel, cyclic)
        except SystemError as e:
            raise DeviceError(str(e))
        except RuntimeError as e:
            write_timeout_err = 'data write timeout'
            sample_drop_err = 'data sample dropped'
            if e.message[:len(sample_drop_err)] == sample_drop_err:
                if not self.ignore_dataflow:
                    raise SampleDrop(e.message)
            elif e.message[:len(write_timeout_err)] == write_timeout_err:
                raise WriteTimeout(e.message)
            else:
                raise DeviceError(str(e))

        if ret < 0:
            raise DeviceError('failed writing to device', ret)

    def flush(self, int channel, bint read=False):
        """Flush the selected channel's write queue and optionally the read queue for the device.

        Args:
            channel (int): channel write queue to flush, use -1 to skip flushing write queue
            read (bool, optional): whether to flush the incoming read queue
        """
        self._device.flush(channel, read)

    def get_samples(self, num_samples):
        """Acquire all signal samples from a device in a non-continuous fashion.

        Blocks until the requested number of samples is available.

        Note that this should mainly be used for single device sessions as it
        calls session.run() internally. Use session.get_samples() directly for
        a more robust solution.

        Args:
            num_samples (int): number of samples to read

        Example usage with at least one device plugged in:

        >>> from pysmu import Session, Mode
        >>> session = Session()
        >>> dev = session.devices[0]
        >>> samples = dev.get_samples(10))
        >>> assert len(samples) == 10

        Two channels worth of data.
        >>> assert len(samples[0]) == 2

        Each channel's sample contains a voltage and current value.
        >>> assert len(samples[0][0]) == 2

        Raises: DeviceError on reading failures.
        Returns: A list containing the specified number of sample values.
        """
        try:
            return self._session.get_samples(num_samples)[self._session_index]
        except IndexError:
            raise DeviceError('device detached')

    def flash_firmware(self, path):
        """Update firmware for the device.

        Attributes:
            path (str): Path to firmware file.

        >>> from pysmu import Session
        >>> session = Session()
        >>> dev = session.devices[0]
        >>> dev.flash_firmware('/path/to/m1000-2.06.bin')
        >>> assert dev.fwver == '2.06'

        Raises: DeviceError on writing failures.
        """
        try:
            self._session.flash_firmware(path.encode(), (self,))
        except SessionError as e:
            raise DeviceError(str(e))


cdef class Channel:
    cdef Device dev
    cdef int chan

    def __init__(self, Device device, int channel):
        self.dev = device
        self.chan = channel

    property mode:
        """Get/set the mode of the channel.

        Example usage with at least one device plugged in:

        >>> from pysmu import Session, Mode
        >>> session = Session()
        >>> dev = session.devices[0]
        >>> print(dev.channels['A'].mode)
        Mode.HI_Z
        >>> dev.channels['A'].mode = Mode.SVMI
        >>> print(dev.channels['A'].mode)
        Mode.SVMI

        Raises: ValueError if an invalid mode is passed.
        Raises: DeviceError on failure.
        """
        def __get__(self):
            cdef int mode
            mode = self.dev._device.get_mode(self.chan)
            return Mode(mode)

        def __set__(self, mode):
            if mode not in Mode:
                raise ValueError('invalid mode: {}'.format(mode))

            cdef int ret = 0
            ret = self.dev._device.set_mode(self.chan, mode.value)
            if ret:
                raise DeviceError('failed setting mode {}: '.format(mode), ret)

    property signal:
        """Get the signal for the channel."""
        def __get__(self):
            if self.mode == Mode.SIMV:
                return _DeviceSignal._create(self.dev._device.signal(self.chan, 1))
            else:
                return _DeviceSignal._create(self.dev._device.signal(self.chan, 0))

    def flush(self):
        """Flush the channel's write queue."""
        self.dev.flush(self.chan)

    def read(self, num_samples, timeout=0):
        """Acquire samples from a channel.

        Args:
            num_samples (int): number of samples to read
            timeout (int, optional): amount of time in milliseconds to wait for samples to be available.
                - If 0 (the default), return immediately.
                - If -1, block indefinitely until the requested number of samples is returned.

        Raises: DeviceError on reading failures.
        Returns: A list containing sample values from the channel.
        """
        return [x[self.chan] for x in self.dev.read(num_samples, timeout=timeout)]

    def write(self, data, cyclic=False):
        """Write data to the channel.

        Args:
            data: iterable of sample values
            cyclic (bool, optional): continuously iterate over the same buffer

        Raises: DeviceError on writing failures.
        """
        self.dev.write(data, channel=self.chan, cyclic=cyclic)

    def get_samples(self, num_samples):
        """Acquire samples from a channel in a non-continuous fashion.

        Blocks until the requested number of samples is available.

        Note that this should mainly be used for single device sessions as it
        calls session.run() internally. Use session.get_samples() directly for
        a more robust solution.

        Args:
            num_samples (int): number of samples to read

        Example usage with at least one device plugged in:

        >>> from pysmu import Session, Mode
        >>> session = Session()
        >>> dev = session.devices[0]
        >>> chan_a = dev.channels['A']
        >>> samples = chan_a.get_samples(10))
        >>> assert len(samples) == 10

        Each sample contains a voltage and current value.
        >>> assert len(samples[0]) == 2

        >>> print(samples)
        [(0.146484375, 0.0003067016659770161),
         (0.146484375, 0.0014099121326580644),
         (0.1467132568359375, 0.0012573242420330644),
         (0.146636962890625, 0.0010742187732830644),
         (0.1468658447265625, 0.0010437011951580644),
         (0.14678955078125, 0.0012268066639080644),
         (0.1467132568359375, 0.0012573242420330644),
         (0.1467132568359375, 0.0015014648670330644),
         (0.1467132568359375, 0.0012878418201580644),
         (0.146636962890625, 0.0014404297107830644)]
        """
        return [x[self.chan] for x in self.dev.get_samples(num_samples)]

    def arbitrary(self, waveform, cyclic=False):
        """Output an arbitrary waveform.

        Args:
            waveform: sequence of raw waveform values (floats or ints)
            cyclic (boolean, optional): repeat the waveform when arriving at its end

        Example waveform writing to a channel:

        >>> from pysmu import Session, Mode
        >>> session = Session()
        >>> dev = session.devices[0]
        >>> chan_a = dev.channels['A']
        >>> chan_a.mode = Mode.SVMI
        >>> chan_a.arbitrary([1,3,5,3,1], cyclic=True)
        >>> print(chan_a.get_samples(10))
        [1.6732025146484375,
         3.3382415771484375,
         4.1905975341796875,
         2.4416351318359375,
         1.12823486328125,
         1.55853271484375,
         3.3242034912109375,
         4.1870880126953125,
         2.441253662109375,
         1.126708984375]
        """
        self.write(waveform, cyclic=cyclic)

    def constant(self, value, cyclic=True):
        """Set output to a constant waveform.

        Attributes:
            value: Constant value to set the channel to.
            cyclic (boolean, optional): repeat the waveform when arriving at its end

        Example waveform writing to a channel:

        >>> from pysmu import Session, Mode
        >>> session = Session()
        >>> dev = session.devices[0]
        >>> chan_a = dev.channels['A']
        >>> chan_a.mode = Mode.SVMI
        >>> chan_a.constant(4)
        >>> print(chan_a.get_samples(10))
        [3.905792236328125,
         3.905792236328125,
         3.9055633544921875,
         3.9054107666015625,
         3.9055633544921875,
         3.905487060546875,
         3.905487060546875,
         3.90533447265625,
         3.9054107666015625,
         3.90533447265625]
        """
        data = self.signal.constant(1000, value)
        self.write(data, cyclic=cyclic)

    def square(self, float midpoint, float peak, double period, double phase, double duty, cyclic=True):
        """Set output to a square waveform.

        Attributes:
            midpoint: value at the middle of the wave
            peak: maximum value of the wave
            period: number of samples the wave takes for one cycle
            phase: position in time (sample number) that the wave starts at
            duty: duty cycle of the waveform (fraction of time in which the
                signal is active, e.g. 0.5 is half the time)
            cyclic (boolean, optional): repeat the waveform when arriving at its end

        Example waveform writing to a channel:

        >>> from pysmu import Session, Mode
        >>> session = Session()
        >>> dev = session.devices[0]
        >>> chan_a = dev.channels['A']
        >>> chan_a.mode = Mode.SVMI
        >>> chan_a.square(0, 5, 10, 0, .5)
        >>> print([x[0] for x in chan_a.get_samples(10)])
        [0.0025177001953125,
         0.0020599365234375,
         0.0016021728515625,
         0.0014495849609375,
         0.60089111328125,
         4.106903076171875,
         4.7507476806640625,
         4.852752685546875,
         4.871063232421875,
         4.1985321044921875]
        """
        data = self.signal.square(period, midpoint, peak, period, phase, duty)
        self.write(data, cyclic=cyclic)

    def sawtooth(self, float midpoint, float peak, float period, float phase, cyclic=True):
        """Set output to a sawtooth waveform.

        Attributes:
            midpoint: value at the middle of the wave
            peak: maximum value of the wave
            period: number of samples the wave takes for one cycle
            phase: position in time (sample number) that the wave starts at
            cyclic (boolean, optional): repeat the waveform when arriving at its end

        Example waveform writing to a channel:

        >>> from pysmu import Session, Mode
        >>> session = Session()
        >>> dev = session.devices[0]
        >>> chan_a = dev.channels['A']
        >>> chan_a.mode = Mode.SVMI
        >>> chan_a.sawtooth(0, 5, 10, 0)
        >>> print([x[0] for x in chan_a.get_samples(10)])
        [4.5452880859375,
         4.163665771484375,
         3.6379241943359375,
         3.08868408203125,
         2.5484466552734375,
         2.0189666748046875,
         1.4780426025390625,
         0.9271240234375,
         0.39886474609375,
         0.6231689453125]
        """
        data = self.signal.sawtooth(period, midpoint, peak, period, phase)
        self.write(data, cyclic=cyclic)

    def stairstep(self, float midpoint, float peak, float period, float phase, cyclic=True):
        """Set output to a stairstep waveform.

        Attributes:
            midpoint: value at the middle of the wave
            peak: maximum value of the wave
            period: number of samples the wave takes for one cycle
            phase: position in time (sample number) that the wave starts at
            cyclic (boolean, optional): repeat the waveform when arriving at its end

        Example waveform writing to a channel:

        >>> from pysmu import Session, Mode
        >>> session = Session()
        >>> dev = session.devices[0]
        >>> chan_a = dev.channels['A']
        >>> chan_a.mode = Mode.SVMI
        >>> chan_a.stairstep(0, 5, 10, 0)
        >>> print([x[0] for x in chan_a.get_samples(10)])
        [4.5502471923828125,
          4.16473388671875,
         3.636932373046875,
         3.0841064453125,
         2.5444793701171875,
         2.017669677734375,
         1.48040771484375,
         0.9300994873046875,
         0.40069580078125,
         0.623779296875]
        """
        data = self.signal.stairstep(period, midpoint, peak, period, phase)
        self.write(data, cyclic=cyclic)

    def sine(self, float midpoint, float peak, float period, float phase, cyclic=True):
        """Set output to a sinusoidal waveform.

        Attributes:
            midpoint: value at the middle of the wave
            peak: maximum value of the wave
            period: number of samples the wave takes for one cycle
            phase: position in time (sample number) that the wave starts at
            cyclic (boolean, optional): repeat the waveform when arriving at its end

        Example waveform writing to a channel:

        >>> from pysmu import Session, Mode
        >>> session = Session()
        >>> dev = session.devices[0]
        >>> chan_a = dev.channels['A']
        >>> chan_a.mode = Mode.SVMI
        >>> chan_a.sine(0, 5, 10, 0)
        >>> print([x[0] for x in chan_a.get_samples(10)])
        [4.3698883056640625,
         3.99261474609375,
         2.756195068359375,
         1.3651275634765625,
         0.4055023193359375,
         0.2043914794921875,
         0.859222412109375,
         2.1010589599609375,
         3.5059356689453125,
         4.4654083251953125]
        """
        data = self.signal.sine(period, midpoint, peak, period, phase)
        self.write(data, cyclic=cyclic)

    def triangle(self, float midpoint, float peak, float period, float phase, cyclic=True):
        """Set output to a triangular waveform.

        Attributes:
            midpoint: value at the middle of the wave
            peak: maximum value of the wave
            period: number of samples the wave takes for one cycle
            phase: position in time (sample number) that the wave starts at
            cyclic (boolean, optional): repeat the waveform when arriving at its end

        Example waveform writing to a channel:

        >>> from pysmu import Session, Mode
        >>> session = Session()
        >>> dev = session.devices[0]
        >>> chan_a = dev.channels['A']
        >>> chan_a.mode = Mode.SVMI
        >>> chan_a.triangle(0, 5, 10, 0)
        >>> print([x[0] for x in chan_a.get_samples(10)])
        [4.27337646484375,
         3.5883331298828125,
         2.6357269287109375,
         1.683807373046875,
         0.729522705078125,
         0.378570556640625,
         1.2639617919921875,
         2.2304534912109375,
         3.199462890625,
         4.13421630859375]
        """
        data = self.signal.triangle(period, midpoint, peak, period, phase)
        self.write(data, cyclic=cyclic)


cdef class Signal:
    # pointer to the underlying C++ smu::Signal object
    cdef cpp_libsmu.Signal *_signal

    def __cinit__(self):
        self._signal = new cpp_libsmu.Signal()

        if self._signal is NULL:
            raise MemoryError()

    def constant(self, uint64_t samples, float val):
        """Generate a constant waveform.

        Attributes:
            samples: number of samples to generate for the waveform
            val: contant value for the waveform

        Example waveform generation:

        >>> from pysmu import Signal
        >>> signal = Signal()
        >>> constant_wave = signal.constant(10, 2)
        >>> assert len(constant) == 10
        >>> print(stairstep_wave)
        [2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0]
        """
        cdef vector[float] buf
        self._signal.constant(buf, samples, val)
        return buf

    def square(self, uint64_t samples, float midpoint, float peak, double period, double phase, double duty):
        """Generate a square waveform.

        Attributes:
            samples: number of samples to generate for the waveform
            midpoint: value at the middle of the wave
            peak: maximum value of the wave
            period: number of samples the wave takes for one cycle
            phase: position in time (sample number) that the wave starts at
            duty: duty cycle of the waveform (fraction of time in which the
                signal is active, e.g. 0.5 is half the time)

        Example waveform generation:

        >>> from pysmu import Signal
        >>> signal = Signal()
        >>> square_wave = signal.square(10, 0, 5, 10, 0, .5)
        >>> assert len(square_wave) == 10
        >>> print(square_wave)
        [0.0, 0.0, 0.0, 0.0, 0.0, 5.0, 5.0, 5.0, 5.0, 5.0]
        """
        cdef vector[float] buf
        self._signal.square(buf, samples, midpoint, peak, period, phase, duty)
        return buf

    def sawtooth(self, uint64_t samples, float midpoint, float peak, double period, double phase):
        """Generate a sawtooth waveform.

        Attributes:
            samples: number of samples to generate for the waveform
            midpoint: value at the middle of the wave
            peak: maximum value of the wave
            period: number of samples the wave takes for one cycle
            phase: position in time (sample number) that the wave starts at

        Example waveform generation:

        >>> from pysmu import Signal
        >>> signal = Signal()
        >>> sawtooth_wave = signal.sawtooth(10, 0, 5, 10, 0)
        >>> assert len(sawtooth_wave) == 10
        >>> print(sawtooth_wave)
        [5.0,
         4.44444465637207,
         3.8888888359069824,
         3.3333332538604736,
         2.777777671813965,
         2.222222089767456,
         1.6666665077209473,
         1.1111111640930176,
         0.5555553436279297,
         0.0]
        """
        cdef vector[float] buf
        self._signal.sawtooth(buf, samples, midpoint, peak, period, phase)
        return buf

    def stairstep(self, uint64_t samples, float midpoint, float peak, double period, double phase):
        """Generate a stairstep waveform.

        Attributes:
            samples: number of samples to generate for the waveform
            midpoint: value at the middle of the wave
            peak: maximum value of the wave
            period: number of samples the wave takes for one cycle
            phase: position in time (sample number) that the wave starts at

        Example waveform generation:

        >>> from pysmu import Signal
        >>> signal = Signal()
        >>> stairstep_wave = signal.stairstep(10, 0, 5, 10, 0)
        >>> assert len(stairstep_wave) == 10
        >>> print(stairstep_wave)
        [5.0,
         4.44444465637207,
         3.8888888359069824,
         3.3333334922790527,
         2.777777671813965,
         2.222222328186035,
         1.6666667461395264,
         1.1111111640930176,
         0.5555553436279297,
         0.0]
        """
        cdef vector[float] buf
        self._signal.stairstep(buf, samples, midpoint, peak, period, phase)
        return buf

    def sine(self, uint64_t samples, float midpoint, float peak, double period, double phase):
        """Generate a sinusoidal waveform.

        Attributes:
            samples: number of samples to generate for the waveform
            midpoint: value at the middle of the wave
            peak: maximum value of the wave
            period: number of samples the wave takes for one cycle
            phase: position in time (sample number) that the wave starts at

        Example waveform generation:

        >>> from pysmu import Signal
        >>> signal = Signal()
        >>> sine_wave = signal.sine(10, 0, 5, 10, 0)
        >>> assert len(sine_wave) == 10
        >>> print(sine_wave)
        [5.0,
         4.522542476654053,
         3.2725424766540527,
         1.7274575233459473,
         0.47745752334594727,
         0.0,
         0.47745752334594727,
         1.7274575233459473,
         3.2725424766540527,
         4.522542476654053]
        """
        cdef vector[float] buf
        self._signal.sine(buf, samples, midpoint, peak, period, phase)
        return buf

    def triangle(self, uint64_t samples, float midpoint, float peak, double period, double phase):
        """Generate a triangular waveform.

        Attributes:
            samples: number of samples to generate for the waveform
            midpoint: value at the middle of the wave
            peak: maximum value of the wave
            period: number of samples the wave takes for one cycle
            phase: position in time (sample number) that the wave starts at

        Example waveform generation:

        >>> from pysmu import Signal
        >>> signal = Signal()
        >>> triangle_wave = signal.triangle(10, 0, 5, 10, 0)
        >>> assert len(triangle_wave) == 10
        >>> print(triangle_wave)
        [5.0, 4.0, 3.0, 2.0, 1.0, 0.0, 1.0, 2.0, 3.0, 4.0]
        """
        cdef vector[float] buf
        self._signal.triangle(buf, samples, midpoint, peak, period, phase)
        return buf


cdef class _DeviceSignal(Signal):
    """Wrapper for device specific signal properties."""

    property label:
        """Get the signal's label.

        >>> from pysmu import Session, Mode
        >>> dev = session.devices[0]
        >>> chan_a = dev.channels['A']
        >>> chan_a.mode = Mode.SVMI
        >>> chan_a.signal.label
        Voltage
        >>> chan_a.mode = Mode.SIMV
        >>> chan_a.signal.label
        Current
        """
        def __get__(self):
            return self._signal.info().label

    property min:
        """Get the signal's minimum value.

        >>> from pysmu import Session, Mode
        >>> dev = session.devices[0]
        >>> chan_a = dev.channels['A']
        >>> chan_a.mode = Mode.SVMI
        >>> chan_a.signal.min
        0.0
        >>> chan_a.mode = Mode.SIMV
        >>> chan_a.signal.min
        -0.2
        """
        def __get__(self):
            return self._signal.info().min

    property max:
        """Get the signal's maximum value.

        >>> from pysmu import Session, Mode
        >>> dev = session.devices[0]
        >>> chan_a = dev.channels['A']
        >>> chan_a.mode = Mode.SVMI
        >>> chan_a.signal.max
        5.0
        >>> chan_a.mode = Mode.SIMV
        >>> chan_a.signal.max
        0.2
        """
        def __get__(self):
            return self._signal.info().max

    @staticmethod
    cdef _create(cpp_libsmu.Signal *signal) with gil:
        """Internal method to wrap C++ smu::Signal objects."""
        s = _DeviceSignal()
        s._signal = signal
        return s
