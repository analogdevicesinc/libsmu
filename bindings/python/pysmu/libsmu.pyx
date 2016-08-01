# distutils: language = c++

from libcpp.vector cimport vector

cimport cpp_libsmu

from .exceptions import SessionError

__version__ = cpp_libsmu.libsmu_version_str().decode()


cdef extern from "Python.h" nogil:
    void PyEval_InitThreads()


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
        if self._session.scan():
            raise SessionError('failed scanning for supported devices')

    def add_all(self):
        """Scan the system and add all supported devices to the session.

        Raises: SessionError on failure.
        """
        if self._session.add_all():
            raise SessionError('failed scanning and/or adding all supported devices')

    def add(self, Device dev):
        """Add a device to the session.

        Raises: SessionError on failure.
        """
        cdef cpp_libsmu.Device* device
        device = self._session.add(dev._device)
        if device is NULL:
            raise SessionError('failed adding device ({})'.format(dev.serial))

    def remove(self, Device dev):
        """Remove a device from the session."""
        if self._session.remove(dev._device):
            raise SessionError('failed removing device ({})'.format(dev.serial))

    def destroy(self, Device dev):
        """Drop a device from the ltest of available devices."""
        if self._session.destroy(dev._device):
            raise SessionError('failed destroying device ({})'.format(dev.serial))

    def configure(self, int sample_rate):
        """Configure the session's sample rate.

        Attributes:
            sample_rate (int): Sample rate to run the session at.

        Raises: SessionError on failure.
        """
        if self._session.configure(sample_rate):
            raise SessionError('failed configuring device')

    def run(self, int samples):
        """Run the configured capture for a certain number of samples.

        Attributes:
            samples (int): Number of samples to run the session for.
                If 0, run in continuous mode.
        """
        self._session.run(samples)

    def start(self, int samples):
        """Start the currently configured capture, but do not wait for it to complete.

        Attributes:
            samples (int): Number of samples to capture before stopping.
                If 0, run in continuous mode.
        """
        return self._session.start(samples)

    def cancel(self):
        """Cancel the current capture and block while waiting for completion.

        Raises: SessionError on failure.
        """
        if self._session.cancel():
            raise SessionError('failed canceling device transfers')

    def wait_for_completion(self):
        """Block until all devices have are finished streaming in the session."""
        self._session.wait_for_completion()

    def end(self):
        """Block until all devices have completed, then turn off the devices."""
        self._session.end()

    def flash_firmware(self, file, Device dev=None):
        """Update firmware for a given device.

        Attributes:
            file (str): Path to firmware file.
            dev: The device targeted for updating. If not supplied or None, the
                first attached device in the session will be used.
        """
        cdef cpp_libsmu.Device *device
        if dev is None:
            device = NULL
        else:
            device = dev._device

        return self._session.flash_firmware(file.encode(), device)

    def __dealloc__(self):
        # make sure the session is completed before deallocation
        self._session.end()
        # TODO: determine why usb thread joining stalls for ~20-30 seconds on
        # session destruction.
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

    def write_calibration(self, file):
        """Write calibration data to the device's EEPROM.

        Args:
            file (str): path to calibration file
                (use None to reset the calibration to the defaults)

        Raises: RuntimeError on writing failures.
        """
        cdef const char* cal_path
        if file is None:
            cal_path = NULL
        else:
            file = file.encode()
            cal_path = file

        r = self._device.write_calibration(cal_path)
        if r < 0:
            raise RuntimeError('failed writing device calibration data')

    def __str__(self):
        return 'serial {}: fw {}: hw {}'.format(self.serial, self.fwver, self.hwver)

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
        data = data.encode()

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
