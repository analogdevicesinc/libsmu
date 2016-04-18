// Released under the terms of the BSD License
// (C) 2014-2016
//   Analog Devices, Inc.
//   Kevin Mehall <km@kevinmehall.net>
//   Ian Daniher <itdaniher@gmail.com>

#include "libsmu.hpp"
#include <iostream>
#include <libusb.h>
#include <string.h>
#include "device_cee.hpp"
#include "device_m1000.hpp"

using std::shared_ptr;

extern "C" int LIBUSB_CALL hotplug_callback_usbthread(
	libusb_context *ctx, libusb_device *device, libusb_hotplug_event event, void *user_data);

/// session constructor
Session::Session() {
	m_active_devices = 0;

	if (int r = libusb_init(&m_usb_cx) != 0) {
		smu_debug("libusb init failed: %i\n", r);
		abort();
	}

	if (libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG)) {
		smu_debug("Using libusb hotplug\n");
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
		smu_debug("libusb hotplug cb reg failed: %i\n", r);
	};
	} else {
		smu_debug("Libusb hotplug not supported. Only devices already attached will be used.\n");
	}
	start_usb_thread();

	if (getenv("LIBUSB_DEBUG")) {
		libusb_set_debug(m_usb_cx, 4);
	}
}

/// session destructor
Session::~Session() {
	std::lock_guard<std::mutex> lock(m_lock_devlist);
	// Run device destructors before libusb_exit
	m_usb_thread_loop = 0;
	m_devices.clear();
	m_available_devices.clear();
	if (m_usb_thread.joinable()) {
		m_usb_thread.join();
	}
	libusb_exit(m_usb_cx);
}

/// callback for device attach events
void Session::attached(libusb_device *device) {
	shared_ptr<Device> dev = probe_device(device);
	if (dev) {
		std::lock_guard<std::mutex> lock(m_lock_devlist);
		m_available_devices.push_back(dev);
		smu_debug("Session::attached ser: %s\n", dev->serial());
		if (this->m_hotplug_attach_callback) {
			this->m_hotplug_attach_callback(&*dev);
		}
	}
}

// callback for device detach events
void Session::detached(libusb_device *device)
{
	if (this->m_hotplug_detach_callback) {
		shared_ptr<Device> dev = this->find_existing_device(device);
		if (dev) {
			smu_debug("Session::detached ser: %s\n", dev->serial());
			this->m_hotplug_detach_callback(&*dev);
		}
	}
}


/// remove a specified Device from the list of available devices
void Session::destroy_available(Device *dev) {
	std::lock_guard<std::mutex> lock(m_lock_devlist);
	if (dev)
		for (unsigned i = 0; i < m_available_devices.size(); i++)
			if (m_available_devices[i]->serial() == dev->serial())
				m_available_devices.erase(m_available_devices.begin()+i);
}

/// low-level callback for hotplug events, proxies to session methods
extern "C" int LIBUSB_CALL hotplug_callback_usbthread(
	libusb_context *ctx, libusb_device *device, libusb_hotplug_event event, void *user_data) {
	(void) ctx;
	Session *sess = (Session *) user_data;
	if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED) {
		sess->attached(device);
	} else if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT) {
		sess->detached(device);
	}
	return 0;
}

/// spawn thread for USB transaction handling
void Session::start_usb_thread() {
	m_usb_thread_loop = true;
	m_usb_thread = std::thread([=]() {
		while(m_usb_thread_loop) libusb_handle_events(m_usb_cx);
	});
}

/// update list of attached USB devices
int Session::update_available_devices() {
	m_lock_devlist.lock();
	m_available_devices.clear();
	m_lock_devlist.unlock();
	libusb_device** list;
	int num = libusb_get_device_list(m_usb_cx, &list);
	if (num < 0) return num;

	for (int i=0; i<num; i++) {
		shared_ptr<Device> dev = probe_device(list[i]);
		if (dev) {
			m_lock_devlist.lock();
			m_available_devices.push_back(dev);
			m_lock_devlist.unlock();
		}
	}

	libusb_free_device_list(list, true);
	return 0;
}

/// identify devices supported by libsmu
shared_ptr<Device> Session::probe_device(libusb_device* device) {
	shared_ptr<Device> dev = find_existing_device(device);

	libusb_device_descriptor desc;
	int r = libusb_get_device_descriptor(device, &desc);
	if (r != 0) {
		smu_debug("Error %i in get_device_descriptor\n", r);
		return NULL;
	}

	if (desc.idVendor == 0x59e3 && desc.idProduct == 0xCEE1) {
		dev = shared_ptr<Device>(new CEE_Device(this, device));
	} else if (desc.idVendor == 0x0456 && desc.idProduct == 0xCEE2) {
		dev = shared_ptr<Device>(new M1000_Device(this, device));
	} else if (desc.idVendor == 0x064B && desc.idProduct == 0x784C) {
		dev = shared_ptr<Device>(new M1000_Device(this, device));
	}

	if (dev) {
		if (dev->init() == 0) {
			libusb_get_string_descriptor_ascii(dev->m_usb, desc.iSerialNumber, (unsigned char*)&dev->serial_num, 32);
			libusb_control_transfer(dev->m_usb, 0xC0, 0x00, 0, 0, (unsigned char*)&dev->m_hw_version, 64, 100);
			libusb_control_transfer(dev->m_usb, 0xC0, 0x00, 0, 1, (unsigned char*)&dev->m_fw_version, 64, 100);

			return dev;
		} else {
			perror("Error initializing device");
		}
	}
	return NULL;
}

shared_ptr<Device> Session::find_existing_device(libusb_device* device) {
	std::lock_guard<std::mutex> lock(m_lock_devlist);
	for (auto d: m_available_devices) {
		if (d->m_device == device) {
			return d;
		}
	}
	return NULL;
}

/// get the device matching a given serial from the session
Device* Session::get_device(const char* serial) {
	for (auto d: m_devices) {
		if (strncmp(d->serial(), serial, 31) == 0) {
			return d;
		}
	}
	return NULL;
}

/// adds a new device to the session
Device* Session::add_device(Device* device) {
	if ( device ) {
		m_devices.insert(device);
		smu_debug("device insert: %s\n", device->serial());
		device->added();
		return device;
	}
	return NULL;
}

/// removes an existing device from the session
void Session::remove_device(Device* device) {
	if ( device ) {
		m_devices.erase(device);
		device->removed();
	}
	else {
		smu_debug("no device removed\n");
	}
}

/// configures sampling for all devices
void Session::configure(uint64_t sampleRate) {
	for (auto i: m_devices) {
		i->configure(sampleRate);
	}
}

/// stream nsamples, then stop
void Session::run(sample_t nsamples) {
	start(nsamples);
	end();
}

/// wait for completion of sample stream, disable all devices
void Session::end() {
	// completion lock
	std::unique_lock<std::mutex> lk(m_lock);
	auto now = std::chrono::system_clock::now();
	auto res = m_completion.wait_until(lk, now + std::chrono::milliseconds(1000), [&]{ return m_active_devices == 0; });
	//	m_completion.wait(lk, [&]{ return m_active_devices == 0; });
	// wait on m_completion, return m_active_devices compared with 0
	if (!res) {
		smu_debug("timed out\n");
	}
	for (auto i: m_devices) {
		i->off();
	}
}
/// wait for completion of sample stream
void Session::wait_for_completion() {
	// completion lock
	std::unique_lock<std::mutex> lk(m_lock);
	m_completion.wait(lk, [&]{ return m_active_devices == 0; });
}

/// start streaming data
void Session::start(sample_t nsamples) {
	m_min_progress = 0;
	m_cancellation = 0;
	for (auto i: m_devices) {
		i->on();
		if (m_devices.size() > 1) {
			i->sync();
		}
		i->start_run(nsamples);
		m_active_devices += 1;
	}
}

/// cancel all pending USB transactions
void Session::cancel() {
	m_cancellation = LIBUSB_TRANSFER_CANCELLED;
	for (auto i: m_devices) {
		i->cancel();
	}
}

/// Called on the USB thread when a device encounters an error
void Session::handle_error(int status, const char * tag) {
	std::lock_guard<std::mutex> lock(m_lock);
	// a canceled transfer completing is not an error...
	if ((m_cancellation == 0) && (status != LIBUSB_TRANSFER_CANCELLED) ) {
		smu_debug("error condition at %s: %s\n", tag, libusb_error_name(status));
		m_cancellation = status;
		cancel();
	}
}

/// called upon completion of a sample stream
void Session::completion() {
	// On USB thread
	m_active_devices -= 1;
	std::lock_guard<std::mutex> lock(m_lock);
	if (m_active_devices == 0) {
		if (m_completion_callback) {
			m_completion_callback(m_cancellation != 0);
		}
		m_completion.notify_all();
	}
}

void Session::progress() {
	sample_t min_progress = ULLONG_MAX;
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

Device::Device(Session* s, libusb_device* d): m_session(s), m_device(d) {
	libusb_ref_device(m_device);
}

// generic device init - libusb_open
int Device::init() {
	int r = libusb_open(m_device, &m_usb);
	return r;
}

// generic device teardown - libusb_close
Device::~Device() {
	if (m_usb)
		libusb_close(m_usb);
	if (m_device)
		libusb_unref_device(m_device);
}

// generic implementation of ctrl_transfers
int Device::ctrl_transfer(unsigned bmRequestType, unsigned bRequest, unsigned wValue, unsigned wIndex, unsigned char *data, unsigned wLength, unsigned timeout)
{
	return libusb_control_transfer(m_usb, bmRequestType, bRequest, wValue, wIndex, data, wLength, timeout);
}
