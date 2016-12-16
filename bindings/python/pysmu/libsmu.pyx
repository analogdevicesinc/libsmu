# distutils: language = c++

from collections import OrderedDict

from libc.stdint cimport uint32_t
from libcpp.vector cimport vector

# enum is only in py34 and up, use vendored backport if the system doesn't have
# it available for py27
try:
    from enum import Enum
except ImportError:
    from ._vendor.enum import Enum

cimport cpp_libsmu
from .array cimport array
from .exceptions import *

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

    def __init__(self, add_all=True, ignore_dataflow=False, sample_rate=0):
        """Initialize a session.

        Attributes:
            add_all (bool, optional): Add all attached devices to the session on initialization.
            ignore_dataflow (bool, optional): Ignore sample drops or write timeouts for all
                devices in the session.
            sample_rate (int, optional): Sample rate to run the session at.
                A sample rate of 0 (the default) causes the session to use the
                default sample rate.
        """
        if add_all:
            self.add_all()

        self.ignore_dataflow = ignore_dataflow
        self.configure(sample_rate)

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
            return tuple(Device._create(d, self.ignore_dataflow) for d
                         in self._session.m_available_devices)

    property devices:
        """Devices that are included in this session."""
        def __get__(self):
            return tuple(Device._create(d, self.ignore_dataflow) for d
                         in self._session.m_devices)

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

    property sample_rate:
        """Session sample rate."""
        def __get__(self):
            return self._session.m_sample_rate
        def __set__(self, rate):
            self.configure(rate)

    def scan(self):
        """Scan the system for supported devices.

        Raises: SessionError on failure.
        """
        cdef int errcode
        errcode = self._session.scan()
        if errcode:
            raise SessionError('failed scanning for supported devices', errcode)

    def add_all(self):
        """Scan the system and add all supported devices to the session.

        Raises: SessionError on failure.
        """
        cdef int errcode
        errcode = self._session.add_all()
        if errcode < 0:
            raise SessionError('failed scanning and/or adding all supported devices', errcode)

    def add(self, Device dev):
        """Add a device to the session.

        Raises: SessionError on failure.
        """
        cdef int errcode
        errcode = self._session.add(dev._device)
        if errcode:
            raise SessionError('failed adding device', errcode)

    def remove(self, Device dev, bint detached=False):
        """Remove a device from the session."""
        cdef int errcode
        errcode = self._session.remove(dev._device, detached)
        if errcode:
            raise SessionError('failed removing device', errcode)

    def destroy(self, Device dev):
        """Drop a device from the list of available devices."""
        cdef int errcode
        errcode = self._session.destroy(dev._device)
        if errcode:
            raise SessionError('failed destroying device', errcode)

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

        cdef int errcode
        errcode = self._session.configure(sample_rate)
        if errcode < 0:
            raise SessionError('failed configuring device', errcode)

    def run(self, int samples):
        """Run the configured capture for a certain number of samples.

        Attributes:
            samples (int): Number of samples to run the session for.
                If 0, run in continuous mode.
        """
        if samples < 0:
            raise ValueError('invalid number of samples: {}'.format(samples))

        cdef int errcode
        errcode = self._session.run(samples)
        if errcode:
            raise SessionError('failed running session stream', errcode)

    def start(self, int samples):
        """Start the currently configured capture, but do not wait for it to complete.

        Attributes:
            samples (int): Number of samples to capture before stopping.
                If 0, run in continuous mode.
        """
        if samples < 0:
            raise ValueError('invalid number of samples: {}'.format(samples))

        cdef int errcode
        errcode = self._session.start(samples)
        if errcode:
            raise SessionError('failed starting session stream', errcode)

    def cancel(self):
        """Cancel the current capture and block while waiting for completion.

        Raises: SessionError on failure.
        """
        cdef int errcode
        errcode = self._session.cancel()
        if errcode:
            raise SessionError('failed canceling device transfers', errcode)

    def flush(self):
        """Flush the read and write queues for all devices in a session."""
        self._session.flush()

    def wait_for_completion(self):
        """Block until all devices have are finished streaming in the session."""
        self._session.wait_for_completion()

    def end(self):
        """Block until all devices have completed, then turn off the devices."""
        cdef int errcode
        errcode = self._session.end()
        if errcode:
            raise SessionError('failed ending session stream', errcode)

    def flash_firmware(self, file, Device dev=None):
        """Update firmware for a given device.

        Attributes:
            file (str): Path to firmware file.
            dev (obj:`Device`, optional): The device targeted for updating. If not supplied or None, the
                first attached device in the session will be used.

        Raises: SessionError on writing failures.
        """
        cdef cpp_libsmu.Device *device
        if dev is None:
            device = NULL
        else:
            device = dev._device

        try:
            return self._session.flash_firmware(file.encode(), device)
        except RuntimeError as e:
            raise SessionError(str(e))

    def __dealloc__(self):
        del self._session


cdef class Device:
    # pointer to the underlying C++ smu::Device object
    cdef cpp_libsmu.Device *_device
    cdef public bint ignore_dataflow
    cdef readonly object channels

    def __init__(self, ignore_dataflow=False):
        """Initialize a device.

        Attributes:
            ignore_dataflow (bool): Ignore sample drops or write timeouts for the device.
        """
        self.ignore_dataflow = ignore_dataflow
        self.channels = OrderedDict([
            ('A', Channel(self, 0)),
            ('B', Channel(self, 1)),
        ])

    @staticmethod
    cdef _create(cpp_libsmu.Device *device, ignore_dataflow=False) with gil:
        """Internal method to wrap C++ smu::Device objects."""
        d = Device(ignore_dataflow=ignore_dataflow)
        d._device = device
        return d

    property serial:
        """Return device's serial number."""
        def __get__(self):
            return self._device.m_serial.decode()

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

    property samples:
        """Iterable of continuous sampling."""
        def __get__(self):
            while True:
                for x in self.read(1000):
                    yield x

    def read(self, size_t num_samples, int timeout=0):
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
            ret = self._device.read(buf, num_samples, timeout)
        except SystemError as e:
            raise DeviceError(str(e))
        except RuntimeError as e:
            err = 'data sample dropped'
            if not self.ignore_dataflow and e.message[:len(err)] == err:
                raise SampleDrop(err)

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
        cdef int errcode = 0
        cdef vector[float] buf = data

        try:
            errcode = self._device.write(buf, channel, cyclic)
        except SystemError as e:
            raise DeviceError(str(e))
        except RuntimeError as e:
            err = 'data write timeout'
            if not self.ignore_dataflow and e.message[:len(err)] == err:
                raise WriteTimeout(err)

        if errcode < 0:
            raise DeviceError('failed writing to device', errcode)

    def flush(self):
        """Flush the read and write queues for the device."""
        self._device.flush()

    def get_samples(self, num_samples):
        """Acquire all signal samples from a device.

        Blocks until the requested number of samples is available.

        Args:
            num_samples (int): number of samples to read

        Example usage with at least one device plugged in:

        >>> from pysmu import Session, Mode
        >>> session = Session()
        >>> dev = session.devices[0]
        >>> session.run(10)
        >>> samples = dev.get_samples(10))
        >>> assert len(samples) == 10

        Two channels worth of data.
        >>> assert len(samples[0]) == 2

        Each channel's sample contains a voltage and current value.
        >>> assert len(samples[0][0]) == 2

        Raises: DeviceError on reading failures.
        Returns: A list containing the specified number of sample values.
        """
        return self.read(num_samples, -1)

    def write_calibration(self, file):
        """Write calibration data to the device's EEPROM.

        Args:
            file (str): path to calibration file
                (use None to reset the calibration to the defaults)

        Raises: DeviceError on writing failures.
        """
        cdef const char* cal_path
        if file is None:
            cal_path = NULL
        else:
            file = file.encode()
            cal_path = file

        r = self._device.write_calibration(cal_path)
        if r < 0:
            raise DeviceError('failed writing device calibration data')

    def __str__(self):
        return 'serial {}: fw {}: hw {}'.format(self.serial, self.fwver, self.hwver)

    def samba_mode(self):
        """Enable SAM-BA bootloader mode on the device."""
        cdef int errcode
        errcode = self._device.samba_mode()
        if errcode:
            raise DeviceError('failed to enable SAM-BA mode', errcode)

    def set_led(self, led, status):
        """Set device LEDs on or off.

        Args:
            led: specific LED (red, green, blue) to control
            status (bool): on or off

        Raises: ValueError if an invalid LED is passed.
        Raises: IOError on USB failures.
        """
        if led not in LED:
            raise ValueError('invalid LED: {}'.format(led))

        if status:
            # on
            req = 0x50
        else:
            # off
            req = 0x51

        if led == LED.all:
            # toggle all LEDs together
            for x in (l for l in LED if l != LED.all):
                self.ctrl_transfer(0x40, req, x.value, 0, 0, 0, 100)
        else:
            self.ctrl_transfer(0x40, req, led.value, 0, 0, 0, 100)

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
        data = str(data).encode()

        if bm_request_type & 0x80 == 0x80:
            if data == '0':
                data = '\x00' * wLength
        else:
            wLength = 0

        r = self._device.ctrl_transfer(bm_request_type, b_request, wValue,
                                         wIndex, data, wLength, timeout)
        if r < 0:
            raise IOError(abs(r), 'USB control transfer failed')
        else:
            if bm_request_type & 0x80 == 0x80:
                return map(ord, data)
            else:
                return r


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

            cdef int errcode
            errcode = self.dev._device.set_mode(self.chan, mode.value)
            if errcode:
                raise DeviceError('failed setting mode {}: '.format(mode), errcode)

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
        """Acquire samples from a channel.

        Blocks until the requested number of samples is available.

        Args:
            num_samples (int): number of samples to read

        Example usage with at least one device plugged in:

        >>> from pysmu import Session, Mode
        >>> session = Session()
        >>> dev = session.devices[0]
        >>> chan_a = dev.channels['A']
        >>> session.run(10)
        >>> samples = chan_a.get_samples(10))
        >>> assert len(samples) == 10

        Each sample contains a voltage and current value.
        >>> assert len(samples[0]) == 2
        """
        return [x[self.chan] for x in self.dev.get_samples(num_samples)]

    property samples:
        """Iterable of continuous sampling."""
        def __get__(self):
            while True:
                for x in self.read(1000):
                    yield x

    def arbitrary(self, waveform, cyclic=False):
        """Output an arbitrary waveform.

        Args:
            waveform: sequence of raw waveform values (floats or ints)
            cyclic (boolean, optional): repeat the waveform when arriving at its end
        """
        self.write(waveform, cyclic=cyclic)

    def constant(self, value):
        """Set output to a constant waveform.

        Example usage with at least one device plugged in:

        >>> from pysmu import Session, Mode
        >>> session = Session()
        >>> dev = session.devices[0]
        >>> chan_a = dev.channels['A']
        >>> chan_a.mode = Mode.SVMI
        >>> chan_a.constant(4)
        >>> session.run(10)
        >>> print(chan_a.get_samples(2))
        [(3.9046478271484375, 0.0003219604550395161), (3.904571533203125, 0.0002914428769145161)]
        """
        data = [value] * 1000
        self.write(data, cyclic=True)
