# Interface wrapper for the libsmu library.
# distutils: language = c++

from libcpp.memory cimport shared_ptr
from libcpp.set cimport set
from libcpp.vector cimport vector


cdef extern from "libsmu/libsmu.hpp" namespace "smu":
    cdef cppclass Session:
        vector[shared_ptr[Device]] m_available_devices
        set[Device*] m_devices
        int m_active_devices
        int m_queue_size
        void (*m_hotplug_attach_callback)(Device* dev)
        void (*m_hotplug_detach_callback)(Device* dev)

        int scan()
        Device* add(Device* dev)
        int add_all()
        Device* get_device(const char* serial)
        void remove(Device* dev)
        void destroy(Device* dev)
        void configure(int sample_rate)
        void run(int samples)
        void start(int samples)
        void cancel()
        bint cancelled()
        void flash_firmware(const char* path, Device* dev) except +
        void wait_for_completion()
        void end()

    cdef cppclass Device:
        const char* serial()
        const char* fwver()
        const char* hwver()
        int set_mode(int channel, int mode)
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
