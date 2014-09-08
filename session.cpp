#include "libsmu.hpp"
#include <iostream>
#include <libusb-1.0/libusb.h>
#include "device_cee.hpp"

using std::cout;
using std::cerr;
using std::endl;
using std::shared_ptr;

Session::Session()
{
	m_active_devices = 0;

	if (int r = libusb_init(&m_usb_cx) != 0) {
		cerr << "libusb init failed: " << r << endl;
	}

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

	}

	if (dev) {
		if (dev->init() == 0) {
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
	if (m_devices.size() == 0) {
		start_usb_thread();
	}
	m_devices.insert(device);
	device->added();
	return device;
}

void Session::remove_device(Device* device) {
	m_devices.erase(device);
	device->removed();
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
	m_completion.wait(lk, [&]{ return m_active_devices == 0; });
	for (auto i: m_devices) {
		i->off();
	}
}

void Session::start(sample_t nsamples) {
	m_min_progress = 0;
	for (auto i: m_devices) {
		i->on();
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
	if (r!=0) { return r; }
	return 0;
}

Device::~Device()
{
	libusb_close(m_usb);
	libusb_unref_device(m_device);
}
