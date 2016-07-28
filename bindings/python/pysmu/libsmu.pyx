# distutils: language = c++

from libcpp.vector cimport vector

cimport cpp_libsmu

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

    def hotplug_attach(self, f):
        self._session.hotplug_attach(self._hotplug_callback, <void*>f)

    def hotplug_detach(self, f):
        self._session.hotplug_detach(self._hotplug_callback, <void*>f)

    @staticmethod
    cdef void _hotplug_callback(cpp_libsmu.Device *device, void *f) with gil:
        d = Device._create(device)
        (<object>f)(d)

    property available_devices:
        def __get__(self):
            devices = []
            for d in self._session.m_available_devices:
                devices.append(Device._create(d.get()))
            return devices

    property devices:
        def __get__(self):
            devices = []
            for d in self._session.m_devices:
                devices.append(Device._create(d))
            return devices

    property active_devices:
        def __get__(self):
            return self._session.m_active_devices

    property queue_size:
        def __get__(self):
            return self._session.m_queue_size
        def __set__(self, size):
            self._session.m_queue_size = size

    property cancelled:
        def __get__(self):
            return self._session.cancelled()

    def scan(self):
        return self._session.scan()

    def add_all(self):
        return self._session.add_all()

    def add(self, Device dev):
        cdef cpp_libsmu.Device* device
        device = self._session.add(dev._device)
        if device is NULL:
            return None
        else:
            return dev

    def remove(self, Device dev):
        self._session.remove(dev._device)

    def destroy(self, Device dev):
        self._session.destroy(dev._device)

    def configure(self, int sample_rate):
        return self._session.configure(sample_rate)

    def run(self, int samples):
        self._session.run(samples)

    def start(self, int samples):
        return self._session.start(samples)

    def cancel(self):
        return self._session.cancel()

    def wait_for_completion(self):
        return self._session.wait_for_completion()

    def end(self):
        return self._session.end()

    def flash_firmware(self, file, Device dev=None):
        return self._session.flash_firmware(file, dev._device)

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
        Returns: True if writing the calibration settings was successful.
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
            raise RuntimeError()
        return True

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
