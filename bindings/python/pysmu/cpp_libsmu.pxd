# Interface wrapper for the libsmu library.
# distutils: language = c++

from libc.stdint cimport uint32_t, uint64_t
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


cdef extern from "libsmu/libsmu.hpp" namespace "smu" nogil:
    cdef cppclass Session:
        vector[Device*] m_available_devices
        set[Device*] m_devices
        int m_active_devices
        int m_queue_size
        int m_sample_rate
        bint m_continuous

        int scan()
        int add(Device* dev)
        int add_all()
        int remove(Device* dev, bint detached)
        int destroy(Device* dev)
        int configure(uint32_t sample_rate)
        int run(int samples) except +
        int start(int samples)
        int cancel()
        bint cancelled()
        void flush()
        int flash_firmware(const char* path, vector[Device*]) except +
        int end()

    cdef cppclass Device:
        string m_serial
        string m_fwver
        string m_hwver
        bint m_overcurrent

        Signal* signal(unsigned channel, unsigned signal)
        int set_mode(int channel, int mode)
        int get_mode(int channel)
        int fwver_sem(array[unsigned, three]& components)
        int set_serial(string serial)
        ssize_t read(vector[array[float, four]]& buf, size_t samples, int timeout, bint skipsamples) except +
        int write(vector[float]& buf, unsigned channel, bint cyclic) except +
        void flush(int channel, bint read)
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
        int set_led(unsigned leds)
        int set_adc_mux(unsigned adc_mux)

    cdef cppclass Signal:
        sl_signal_info* info()
        void constant(vector[float]& buf, uint64_t samples, float val)
        void square(vector[float]& buf, uint64_t samples, float midpoint, float peak, double period, double phase, double duty)
        void sawtooth(vector[float]& buf, uint64_t samples, float midpoint, float peak, double period, double phase)
        void stairstep(vector[float]& buf, uint64_t samples, float midpoint, float peak, double period, double phase)
        void sine(vector[float]& buf, uint64_t samples, float midpoint, float peak, double period, double phase)
        void triangle(vector[float]& buf, uint64_t samples, float midpoint, float peak, double period, double phase)

    ctypedef struct sl_signal_info:
        const char* label
        uint32_t inputModes
        uint32_t outputModes
        double min
        double max
        double resolution
