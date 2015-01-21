// Released under the terms of the BSD License
// (C) 2014-2015
//   Kevin Mehall <km@kevinmehall.net>
//   Ian Daniher <itdaniher@gmail.com>

#include "libsmu.hpp"
#include <iostream>
#include <libusb-1.0/libusb.h>
#include "device_cee.hpp"
#include "device_m1000.hpp"

using std::cout;
using std::cerr;
using std::endl;
using std::shared_ptr;

extern "C" int LIBUSB_CALL hotplug_callback_usbthread(
    libusb_context *ctx, libusb_device *device, libusb_hotplug_event event, void *user_data);

Session::Session()
{
	m_active_devices = 0;

	if (int r = libusb_init(&m_usb_cx) != 0) {
		cerr << "libusb init failed: " << r << endl;
	}
	if (libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG)) {
        cerr << "Using libusb hotplug" << endl;
        if (int r = libusb_hotplug_register_callback(NULL,
            (libusb_hotplug_event)(LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT),
            (libusb_hotplug_flag) 0,
            LIBUSB_HOTPLUG_MATCH_ANY,
            LIBUSB_HOTPLUG_MATCH_ANY,
            LIBUSB_HOTPLUG_MATCH_ANY,
            hotplug_callback_usbthread,
            this,
            NULL
        ) != 0) {
		cerr << "libusb hotplug cb reg failed: " << r << endl;
	};
    } else {
        cerr << "Libusb hotplug not supported. Only devices already attached will be used." << endl;
	}
	start_usb_thread();

	if (getenv("LIBUSB_DEBUG")) {
		libusb_set_debug(m_usb_cx, 4);
	}
}

Session::~Session()
{
	// Run device destructors before libusb_exit
	m_usb_thread_loop = 0;
	m_devices.clear();
	m_available_devices.clear();
	if (m_usb_thread.joinable()) {
		m_usb_thread.join();
	}
	libusb_exit(m_usb_cx);
}

void Session::attached(libusb_device *device)
{
	shared_ptr<Device> dev = probe_device(device);
	if (m_available_devices.size()) {
		m_available_devices.pop_back();
	}
	m_available_devices.push_back(dev);
	cerr << "ser: " << dev->serial() << endl;
	if (dev) {
		if (this->m_hotplug_attach_callback) {
			this->m_hotplug_attach_callback();
		}
	}
}

void Session::detached(libusb_device *device)
{
	this->remove_device(&(*this->find_existing_device(device)));
	if (this->m_hotplug_detach_callback) {
		this->m_hotplug_detach_callback();
	}
}

extern "C" int LIBUSB_CALL hotplug_callback_usbthread(
    libusb_context *ctx, libusb_device *device, libusb_hotplug_event event, void *user_data) {
	Session *sess = (Session *) user_data;
    if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED) {
		sess->attached(device);
    } else if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT) {
		sess->detached(device);
    }
    return 0;
}

void Session::start_usb_thread() {
	m_usb_thread_loop = true;
	m_usb_thread = std::thread([=]() {
		while(m_usb_thread_loop) libusb_handle_events(m_usb_cx);
	});
}

int Session::update_available_devices()
{
	m_available_devices.clear();
	libusb_device** list;
	int num = libusb_get_device_list(m_usb_cx, &list);
	if (num < 0) return num;

	for (int i=0; i<num; i++) {
		shared_ptr<Device> dev = probe_device(list[i]);
		if (dev) {
			m_available_devices.push_back(dev);
		}
	}

	libusb_free_device_list(list, true);
	return 0;
}

shared_ptr<Device> Session::probe_device(libusb_device* device)
{
	shared_ptr<Device> dev = find_existing_device(device);
	if (dev) {
		return dev;
	}

	libusb_device_descriptor desc;
	int r = libusb_get_device_descriptor(device, &desc);
	if (r != 0) {
		cerr << "Error " << r << "in get_device_descriptor" << endl;
		return NULL;
	}

	if (desc.idVendor == 0x59e3 && desc.idProduct == 0xCEE1) {
		dev = shared_ptr<Device>(new CEE_Device(this, device));
	} else if (desc.idVendor == 0x0456 && desc.idProduct == 0xCEE2) {
		dev = shared_ptr<Device>(new M1000_Device(this, device));
	}

	if (dev) {
		if (dev->init() == 0) {
			libusb_get_string_descriptor_ascii(dev->m_usb, desc.iSerialNumber, (unsigned char*)&dev->serial_num, 32);
	 		return dev;
		} else {
			cerr << "Error initializing device" << endl;
		}
	}
	return NULL;
}

shared_ptr<Device> Session::find_existing_device(libusb_device* device)
{
	for (auto d: m_available_devices) {
		if (d->m_device == device) {
			return d;
		}
	}
	return NULL;
}

Device* Session::add_device(Device* device) {
	m_devices.insert(device);
	cerr << "device insert " << device << endl; 
	device->added();
	return device;
}

void Session::remove_device(Device* device) {
	if ( device ) { 
		m_devices.erase(device);
		device->removed();
	}
}

void Session::configure(uint64_t sampleRate) {
	for (auto i: m_devices) {
		i->configure(sampleRate);
	}
}

void Session::run(sample_t nsamples) {
	start(nsamples);
	end();
}

void Session::end() {
	std::unique_lock<std::mutex> lk(m_lock);
	for (auto i: m_devices) {
		i->off();
	}
	m_completion.wait(lk, [&]{ return m_active_devices == 0; });
}

void Session::start(sample_t nsamples) {
	m_min_progress = 0;
	for (auto i: m_devices) {
		i->on();
		if (m_devices.size() > 1)
			i->sync();
		i->start_run(nsamples);
		m_active_devices += 1;
	}
}

void Session::cancel() {
	for (auto i: m_devices) {
		i->cancel();
	}
}

void Session::completion() {
	// On USB thread
	std::lock_guard<std::mutex> lock(m_lock);
	m_active_devices -= 1;
	if (m_active_devices == 0) {
		if (m_completion_callback) {
			m_completion_callback();
		}
		m_completion.notify_all();
	}
}

void Session::progress() {
	sample_t min_progress = UINT64_MAX;
	for (auto i: m_devices) {
		if (i->m_in_sampleno < min_progress) {
			min_progress = i->m_in_sampleno;
		}
	}

	if (min_progress > m_min_progress) {
		m_min_progress = min_progress;
		if (m_progress_callback) {
			m_progress_callback(m_min_progress);
		}
	}
}

Device::Device(Session* s, libusb_device* d): m_session(s), m_device(d)
{
	libusb_ref_device(m_device);
}

int Device::init()
{
	int r = libusb_open(m_device, &m_usb);
	return r;
}

Device::~Device()
{
	libusb_close(m_usb);
	libusb_unref_device(m_device);
}

void Device::ctrl_transfer(unsigned bmRequestType, unsigned bRequest, unsigned wValue, unsigned wIndex, unsigned char *data, unsigned wLength, unsigned timeout)
{ 
	libusb_control_transfer(m_usb, bmRequestType, bRequest, wValue, wIndex, data, wLength, timeout);
}

