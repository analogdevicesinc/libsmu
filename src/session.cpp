// Released under the terms of the BSD License
// (C) 2014-2016
//   Analog Devices, Inc.
//   Kevin Mehall <km@kevinmehall.net>
//   Ian Daniher <itdaniher@gmail.com>

#include "debug.hpp"
#include "device_m1000.hpp"
#include <libsmu/session.hpp>
#include <libsmu/libsmu.hpp>

#include <algorithm>
#include <iostream>
#include <fstream>
#include <string.h>

#include <libusb.h>

using std::shared_ptr;

using namespace smu;

extern "C" int LIBUSB_CALL hotplug_callback_usbthread(
	libusb_context *ctx, libusb_device *device, libusb_hotplug_event event, void *user_data);

/// session constructor
Session::Session()
{
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
Session::~Session()
{
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
void Session::attached(libusb_device *device)
{
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

/// Internal function to write raw SAM-BA commands to a libusb handle.
static void samba_usb_write(libusb_device_handle *handle, const char* data) {
	int transferred, ret;
	ret = libusb_bulk_transfer(handle, 0x01, (unsigned char *)data, strlen(data), &transferred, 1);
	if (ret < 0) {
		std::string libusb_error_str(libusb_strerror((enum libusb_error)ret));
		throw std::runtime_error("failed to write SAM-BA command: " + libusb_error_str);
	}
}

/// Internal function to read raw SAM-BA commands to a libusb handle.
static void samba_usb_read(libusb_device_handle *handle, unsigned char* data) {
	int transferred, ret;
	ret = libusb_bulk_transfer(handle, 0x82, data, 512, &transferred, 1);
	if (ret < 0) {
		std::string libusb_error_str(libusb_strerror((enum libusb_error)ret));
		throw std::runtime_error("failed to read SAM-BA response: " + libusb_error_str);
	}
}

/// Update device firmware for the specified device or the first device
/// found, either in the current session or in SAMBA command mode.
void Session::flash_firmware(const char *file, Device *dev)
{
	struct libusb_device *usb_dev = NULL;
	struct libusb_device_handle *usb_handle = NULL;
	struct libusb_device **usb_devs;
	struct libusb_device_descriptor usb_info;
	unsigned char usb_data[512];
	unsigned int device_count, page;
	int ret;
	std::vector<uint16_t> samba_device_id;

	std::ifstream firmware (file, std::ios::in | std::ios::binary);
	long firmware_size;
	const uint32_t flashbase = 0x80000;

	if (!dev && this->m_devices.size() > 1) {
		throw std::runtime_error("multiple devices attached, flashing only works on a single device");
	}

	if (!firmware.is_open()) {
		throw std::runtime_error("failed to open firmware file");
	}

	// force attached m1k into command mode
	if (dev || !this->m_devices.empty()) {
		if (!dev)
			dev = *(this->m_devices.begin());
		dev->samba_mode();
	}

	device_count = libusb_get_device_list(NULL, &usb_devs);
	if (device_count <= 0) {
		throw std::runtime_error("error enumerating USB devices");
	}

	// Walk the list of USB devices looking for the device in SAM-BA mode.
	for (unsigned int i = 0; i < device_count; i++) {
		libusb_get_device_descriptor(usb_devs[i], &usb_info);
		samba_device_id = {usb_info.idVendor, usb_info.idProduct};
		if (std::find(SAMBA_DEVICES.begin(), SAMBA_DEVICES.end(), samba_device_id)
				!= SAMBA_DEVICES.end()) {
			// Take the first device found, we disregard multiple devices.
			usb_dev = usb_devs[i];
			break;
		}
	}

	libusb_free_device_list(usb_devs, 1);

	if (usb_dev == NULL) {
		throw std::runtime_error("no supported devices plugged in");
	}

	ret = libusb_open(usb_dev, &usb_handle);
	if (ret < 0) {
		std::string libusb_error_str(libusb_strerror((enum libusb_error)ret));
		throw std::runtime_error("failed opening USB device: " + libusb_error_str);
	}
#ifndef WIN32
	libusb_detach_kernel_driver(usb_handle, 0);
	libusb_detach_kernel_driver(usb_handle, 1);
#endif
	libusb_claim_interface(usb_handle, 1);

	// erase flash
	samba_usb_write(usb_handle, "W400E0804,5A000005#");
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	samba_usb_read(usb_handle, usb_data);
	// check if flash is erased
	samba_usb_write(usb_handle, "w400E0808,4#");
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	samba_usb_read(usb_handle, usb_data);
	samba_usb_read(usb_handle, usb_data);
	samba_usb_read(usb_handle, usb_data);

	// read firmware file into buffer
	firmware.seekg(0, std::ios::end);
	firmware_size = firmware.tellg();
	firmware_size = firmware_size + (256 - firmware_size % 256);
	firmware.seekg(0, std::ios::beg);
	auto buf = std::unique_ptr<char[]>{ new char[firmware_size] };
	firmware.read(buf.get(), firmware_size);
	firmware.close();

	// write firmware
	page = 0;
	char cmd[20];
	uint32_t data;
	for (auto pos = 0; pos < firmware_size; pos += 4) {
		data = (uint8_t)buf[pos] | (uint8_t)buf[pos+1] << 8 |
			(uint8_t)buf[pos+2] << 16 | (uint8_t)buf[pos+3] << 24;
		snprintf(cmd, sizeof(cmd), "W%.8X,%.8X#", flashbase + pos, data);
		samba_usb_write(usb_handle, cmd);
		samba_usb_read(usb_handle, usb_data);
		samba_usb_read(usb_handle, usb_data);
		// On page boundaries, write the page.
		if ((pos & 0xFC) == 0xFC) {
			snprintf(cmd, sizeof(cmd), "W400E0804,5A00%.2X03#", page);
			samba_usb_write(usb_handle, cmd);
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			samba_usb_read(usb_handle, usb_data);
			samba_usb_read(usb_handle, usb_data);
			// Verify page is written.
			samba_usb_write(usb_handle, "w400E0808,4#");
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			samba_usb_read(usb_handle, usb_data);
			samba_usb_read(usb_handle, usb_data);
			samba_usb_read(usb_handle, usb_data);
			// TODO: check page status
			page++;
		}
	}

	// disable SAM-BA
	samba_usb_write(usb_handle, "W400E0804,5A00010B#");
	samba_usb_read(usb_handle, usb_data);
	samba_usb_read(usb_handle, usb_data);
	// jump to flash
	samba_usb_write(usb_handle, "G00000000#");
	samba_usb_read(usb_handle, usb_data);

	libusb_release_interface(usb_handle, 1);
	libusb_close(usb_handle);
}

/// remove a specified Device from the list of available devices
void Session::destroy_available(Device *dev)
{
	std::lock_guard<std::mutex> lock(m_lock_devlist);
	if (dev)
		for (unsigned i = 0; i < m_available_devices.size(); i++)
			if (m_available_devices[i]->serial() == dev->serial())
				m_available_devices.erase(m_available_devices.begin()+i);
}

/// low-level callback for hotplug events, proxies to session methods
extern "C" int LIBUSB_CALL hotplug_callback_usbthread(
	libusb_context *ctx, libusb_device *device, libusb_hotplug_event event, void *user_data)
{
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
void Session::start_usb_thread()
{
	m_usb_thread_loop = true;
	m_usb_thread = std::thread([=]() {
		while(m_usb_thread_loop) libusb_handle_events_completed(m_usb_cx, NULL);
	});
}

/// update list of attached USB devices
int Session::update_available_devices()
{
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
shared_ptr<Device> Session::probe_device(libusb_device* device)
{
	shared_ptr<Device> dev = find_existing_device(device);

	libusb_device_descriptor desc;
	int r = libusb_get_device_descriptor(device, &desc);
	if (r != 0) {
		smu_debug("Error %i in get_device_descriptor\n", r);
		return NULL;
	}

	std::vector<uint16_t> device_id = {desc.idVendor, desc.idProduct};
	if (std::find(SUPPORTED_DEVICES.begin(), SUPPORTED_DEVICES.end(), device_id)
			!= SUPPORTED_DEVICES.end()) {
		dev = shared_ptr<Device>(new M1000_Device(this, device));
	}

	if (dev) {
		if (dev->init() == 0) {
			libusb_get_string_descriptor_ascii(dev->m_usb, desc.iSerialNumber, (unsigned char*)&dev->serial_num, 32);
			dev->ctrl_transfer(0xC0, 0x00, 0, 0, (unsigned char*)&dev->m_hw_version, 64, 100);
			dev->ctrl_transfer(0xC0, 0x00, 0, 1, (unsigned char*)&dev->m_fw_version, 64, 100);
			return dev;
		} else {
			perror("Error initializing device");
		}
	}
	return NULL;
}

shared_ptr<Device> Session::find_existing_device(libusb_device* device)
{
	std::lock_guard<std::mutex> lock(m_lock_devlist);
	for (auto d: m_available_devices) {
		if (d->m_device == device) {
			return d;
		}
	}
	return NULL;
}

/// get the device matching a given serial from the session
Device* Session::get_device(const char* serial)
{
	for (auto d: m_devices) {
		if (strncmp(d->serial(), serial, 31) == 0) {
			return d;
		}
	}
	return NULL;
}

/// adds a new device to the session
Device* Session::add_device(Device* device)
{
	if (device) {
		m_devices.insert(device);
		smu_debug("device insert: %s\n", device->serial());
		device->added();
		return device;
	}
	return NULL;
}

/// removes an existing device from the session
void Session::remove_device(Device* device)
{
	if (device) {
		m_devices.erase(device);
		device->removed();
	}
	else {
		smu_debug("no device removed\n");
	}
}

/// configures sampling for all devices
void Session::configure(uint64_t sampleRate)
{
	for (auto i: m_devices) {
		i->configure(sampleRate);
	}
}

/// stream nsamples, then stop
void Session::run(uint64_t nsamples)
{
	start(nsamples);
	end();
}

/// wait for completion of sample stream, disable all devices
void Session::end()
{
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
void Session::wait_for_completion()
{
	// completion lock
	std::unique_lock<std::mutex> lk(m_lock);
	m_completion.wait(lk, [&]{ return m_active_devices == 0; });
}

/// start streaming data
void Session::start(uint64_t nsamples)
{
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
void Session::cancel()
{
	m_cancellation = LIBUSB_TRANSFER_CANCELLED;
	for (auto i: m_devices) {
		i->cancel();
	}
}

/// Called on the USB thread when a device encounters an error
void Session::handle_error(int status, const char * tag)
{
	std::lock_guard<std::mutex> lock(m_lock);
	// a canceled transfer completing is not an error...
	if ((m_cancellation == 0) && (status != LIBUSB_TRANSFER_CANCELLED) ) {
		smu_debug("error condition at %s: %s\n", tag, libusb_error_name(status));
		m_cancellation = status;
		cancel();
	}
}

/// called upon completion of a sample stream
void Session::completion()
{
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

void Session::progress()
{
	uint64_t min_progress = ULLONG_MAX;
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
