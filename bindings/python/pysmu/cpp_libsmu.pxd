# Interface wrapper for the libsmu library.
# distutils: language = c++

from libcpp.set cimport set
from libcpp.string cimport string
from libcpp.vector cimport vector

from .array cimport array


# Hack to allow integer template parameters, unnecessary when
# https://github.com/cython/cython/pull/426 is merged in some form.
cdef extern from *:
    ctypedef int three "3"
    ctypedef int four "4"


cdef extern from "libsmu/version.hpp":
    const char* libsmu_version_str()


cdef extern from "libsmu/libsmu.hpp" namespace "smu":
    cdef cppclass Session:
        vector[Device*] m_available_devices
        set[Device*] m_devices
        int m_active_devices
        int m_queue_size
        int m_sample_rate

        int scan()
        int add(Device* dev)
        int add_all()
        int remove(Device* dev, bint detached)
        int destroy(Device* dev)
        int configure(int sample_rate)
        int run(int samples)
        int start(int samples)
        int cancel()
        bint cancelled()
        void flush()
        void flash_firmware(const char* path, Device* dev) except +
        void wait_for_completion()
        int end()
        void hotplug_attach(void (Device *dev, void *data) nogil, void *data)
        void hotplug_detach(void (Device *dev, void *data) nogil, void *data)

    cdef cppclass Device:
        string m_serial
        string m_fwver
        string m_hwver

        int set_mode(int channel, int mode)
        int get_mode(int channel)
        int fwver_sem(array[unsigned, three]& components)
        ssize_t read(vector[array[float, four]]& buf, size_t samples, int timeout) except +
        int write(vector[float]& buf, unsigned channel, bint cyclic) except +
        void flush()
        int ctrl_transfer(
            int bmRequestType, int bRequest, int wValue, int wIndex,
            unsigned char* data, int wLength, int timeout)
        int samba_mode()
        int get_default_rate()
        int sync()
        void lock()
        void unlock()
        int write_calibration(const char* path)
        void calibration(vector[vector[float]]* cal)
