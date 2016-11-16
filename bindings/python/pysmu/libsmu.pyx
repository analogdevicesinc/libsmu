# distutils: language = c++

from libcpp.deque cimport deque
from libcpp.vector cimport vector

cimport cpp_libsmu

from .array cimport array
from .exceptions import SessionError, DeviceError

__version__ = cpp_libsmu.libsmu_version_str().decode()


cdef extern from "Python.h" nogil:
    void PyEval_InitThreads()


# workaround only python3.4 and up having native enum support
class Modes(object):
    """Available modes for channels."""
    DISABLED = 0
    SVMI = 1
    SIMV = 2


cdef class Session:
    # pointer to the underlying C++ smu::Session object
    cdef cpp_libsmu.Session *_session

    def __cinit__(self):
        self._session = new cpp_libsmu.Session()

        if self._session is NULL:
            raise MemoryError()

        # initialize/acquire the GIL
        PyEval_InitThreads()

    def hotplug_attach(self, func):
        """Register a function to run on a device attach event.

        Attributes:
            func: Python function to run on device attach events. It should
                accept a single parameter, the device object being attached.
        """
        self._session.hotplug_attach(self._hotplug_callback, <void*>func)

    def hotplug_detach(self, func):
        """Register a function to run on a device detach event.

        Attributes:
            func: Python function to run on device detach events. It should
                accept a single parameter, the device object being detached.
        """
        self._session.hotplug_detach(self._hotplug_callback, <void*>func)

    @staticmethod
    cdef void _hotplug_callback(cpp_libsmu.Device *device, void *func) with gil:
        """Internal proxy to run the python hotplug functions from the C++ side."""
        dev = Device._create(device)
        (<object>func)(dev)

    property available_devices:
        """Devices that are accessible on the system."""
        def __get__(self):
            devices = []
            for d in self._session.m_available_devices:
                devices.append(Device._create(d))
            return devices

    property devices:
        """Devices that are included in this session."""
        def __get__(self):
            devices = []
            for d in self._session.m_devices:
                devices.append(Device._create(d))
            return devices

    property active_devices:
        """Devices that are currently active (streaming data) in this session."""
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
        if errcode:
            raise SessionError('failed scanning and/or adding all supported devices', errcode)

    def add(self, Device dev):
        """Add a device to the session.

        Raises: SessionError on failure.
        """
        cdef int errcode
        errcode = self._session.add(dev._device)
        if errcode:
            raise SessionError('failed adding device', errcode)

    def remove(self, Device dev, detached=False):
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

    def configure(self, int sample_rate):
        """Configure the session's sample rate.

        Attributes:
            sample_rate (int): Sample rate to run the session at.

        Raises: SessionError on failure.
        """
        cdef int errcode
        errcode = self._session.configure(sample_rate)
        if errcode:
            raise SessionError('failed configuring device', errcode)

    def run(self, int samples):
        """Run the configured capture for a certain number of samples.

        Attributes:
            samples (int): Number of samples to run the session for.
                If 0, run in continuous mode.
        """
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
            dev: The device targeted for updating. If not supplied or None, the
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
        # make sure the session is completed before deallocation
        cdef int errcode
        errcode = self._session.end()
        if errcode:
            raise SessionError('failed ending session stream', errcode)

        del self._session


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
            return self._device.serial().decode()

    property fwver:
        """Return device's firmware revision."""
        def __get__(self):
            return self._device.fwver().decode()

    property hwver:
        """Return device's hardware revision."""
        def __get__(self):
            return self._device.hwver().decode()

    property calibration:
        """Read calibration data from the device's EEPROM."""
        def __get__(self):
            cdef vector[vector[float]] cal
            self._device.calibration(&cal)
            return cal;

    def write(self, data, channel, timeout=0):
        """Write data to a specified channel of the device.

        Args:
            data: list or tuple of sample values
            channel (0 or 1): channel to write samples to
            timeout: amount of time in milliseconds to wait for samples
                to be available. If 0 (the default), return immediately.

        Raises: DeviceError on writing failures.
        Returns: The number of samples written.
        """
        cdef ssize_t ret = 0
        cdef deque[float] buf

        for x in data:
            buf.push_back(x)

        try:
            ret = self._device.write(buf, channel, timeout)
        except SystemError as e:
            raise DeviceError(str(e))
        except RuntimeError as e:
            # ignore buffer overflow exceptions
            if not e.message.startswith('dropped '):
                raise

        if ret < 0:
            raise DeviceError('failed writing to device', ret)

        return ret

    def read(self, num_samples, timeout=0):
        """Acquire all signal samples from a device.

        Args:
            num_samples (int): number of samples to read
            timeout: amount of time in milliseconds to wait for samples
                to be available. If 0 (the default), return immediately.

        Raises: DeviceError on reading failures.
        Returns: A list containing the specified number of sample values.
        """
        cdef ssize_t ret = 0
        cdef vector[array[float, cpp_libsmu.four]] buf

        try:
            ret = self._device.read(buf, num_samples, timeout)
        except SystemError as e:
            raise DeviceError(str(e))
        except RuntimeError as e:
            # ignore buffer overflow exceptions
            if not e.message.startswith('dropped '):
                raise

        if ret < 0:
            raise DeviceError('failed writing to device', ret)

        return [(x[0], x[1], x[2], x[3]) for x in buf]

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

    def get_default_rate(self):
        """Get the default sample rate for the device."""
        return self._device.get_default_rate()

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
