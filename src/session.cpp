// Released under the terms of the BSD License
// (C) 2014-2016
//   Analog Devices, Inc.
//   Kevin Mehall <km@kevinmehall.net>
//   Ian Daniher <itdaniher@gmail.com>

#include <algorithm>
#include <iostream>
#include <fstream>
#include <functional>
#include <string.h>

#include <libusb.h>

#include "debug.hpp"
#include "device_m1000.hpp"
#include "usb.hpp"
#include <libsmu/libsmu.hpp>

using namespace std::placeholders;
using namespace smu;

// Callback for libusb hotplug events, proxies to Session::attached() and Session::detached().
extern "C" int LIBUSB_CALL usb_hotplug_callback(
	libusb_context *usb_ctx, libusb_device *usb_dev, libusb_hotplug_event usb_event, void *user_data)
{
	(void) usb_ctx;
	Session *session = (Session *) user_data;
	if (usb_event == LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED) {
		session->attached(usb_dev);
	} else if (usb_event == LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT) {
		session->detached(usb_dev);
	}
	return 0;
}

Session::Session()
{
	m_active_devices = 0;
	int ret;

	ret = libusb_init(&m_usb_ctx);
	if (ret != 0) {
		DEBUG("libusb init failed: %s\n", libusb_error_name(ret));
		abort();
	}

	// Enable USB hotplugging capabilities. If the platform doesn't support
	// this (we're currently using a custom-patched version of libusb to
	// support hotplugging on Windows) we fallback to using all the devices
	// currently plugged in.
	if (libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG)) {
		DEBUG("Using libusb hotplug\n");
		ret = libusb_hotplug_register_callback(
			NULL,
			(libusb_hotplug_event)(LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT),
			(libusb_hotplug_flag) 0,
			LIBUSB_HOTPLUG_MATCH_ANY,
			LIBUSB_HOTPLUG_MATCH_ANY,
			LIBUSB_HOTPLUG_MATCH_ANY,
			usb_hotplug_callback,
			this,
			NULL);
		if (ret != 0)
			DEBUG("libusb hotplug callback registration failed: %s\n", libusb_error_name(ret));
	} else {
		DEBUG("libusb hotplug not supported, only currently attached devices will be used.\n");
	}

	// Spawn a thread to handle pending USB events.
	m_usb_thread_loop = true;
	m_usb_thread = std::thread([=]() {
		while (m_usb_thread_loop) {
			libusb_handle_events_completed(m_usb_ctx, NULL);
		}
	});

	// Enable libusb debugging if LIBUSB_DEBUG is set in the environment.
	if (getenv("LIBUSB_DEBUG")) {
		libusb_set_debug(m_usb_ctx, 4);
	}
}

Session::~Session()
{
	std::lock_guard<std::mutex> lock(m_lock_devlist);
	// stop USB thread loop
	m_usb_thread_loop = false;
	// run device destructors before libusb_exit
	m_devices.clear();
	m_available_devices.clear();
	if (m_usb_thread.joinable()) {
		m_usb_thread.join();
	}
	libusb_exit(m_usb_ctx);
}

void Session::hotplug_attach(std::function<void(Device* device, void* data)> func, void* data)
{
	m_hotplug_attach_callback = std::bind(func, _1, data);
}

void Session::hotplug_detach(std::function<void(Device* device, void* data)> func, void* data)
{
	m_hotplug_detach_callback = std::bind(func, _1, data);
}

void Session::attached(libusb_device *usb_dev)
{
	Device* dev = probe_device(usb_dev);
	if (dev) {
		std::lock_guard<std::mutex> lock(m_lock_devlist);
		m_available_devices.push_back(dev);
		DEBUG("Session::attached device serial: %s\n", dev->serial());
		if (m_hotplug_attach_callback) {
			m_hotplug_attach_callback(dev);
		}
	}
}

void Session::detached(libusb_device *usb_dev)
{
	if (m_hotplug_detach_callback) {
		Device* dev = find_existing_device(usb_dev);
		if (dev) {
			DEBUG("Session::detached device serial: %s\n", dev->serial());
			m_hotplug_detach_callback(dev);
		}
	}
}

// Internal function to write raw SAM-BA commands to a libusb handle.
static void samba_usb_write(libusb_device_handle *usb_handle, const char* data) {
	int transferred, ret;
	ret = libusb_bulk_transfer(usb_handle, 0x01, (unsigned char *)data, strlen(data), &transferred, 1);
	if (ret < 0) {
		std::string libusb_error_str(libusb_strerror((enum libusb_error)ret));
		throw std::runtime_error("failed to write SAM-BA command: " + libusb_error_str);
	}
}

// Internal function to read raw SAM-BA commands to a libusb handle.
static void samba_usb_read(libusb_device_handle *usb_handle, unsigned char* data) {
	int transferred, ret;
	ret = libusb_bulk_transfer(usb_handle, 0x82, data, 512, &transferred, 1);
	if (ret < 0) {
		std::string libusb_error_str(libusb_strerror((enum libusb_error)ret));
		throw std::runtime_error("failed to read SAM-BA response: " + libusb_error_str);
	}
}

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

	if (!dev && m_devices.size() > 1) {
		throw std::runtime_error("multiple devices attached, flashing only works on a single device");
	}

	if (!firmware.is_open()) {
		throw std::runtime_error("failed to open firmware file");
	}

	// TODO: verify that file is a compatible firmware file

	// force attached device into command mode
	if (dev || !m_devices.empty()) {
		if (!dev)
			dev = *(m_devices.begin());
		ret = dev->samba_mode();
		if (ret < 0)
			throw std::runtime_error("failed to enable SAM-BA command mode");
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

	// TODO: verify flashed data

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

int Session::destroy(Device *dev)
{
	std::lock_guard<std::mutex> lock(m_lock_devlist);
	if (dev) {
		for (unsigned i = 0; i < m_available_devices.size(); i++) {
			if (m_available_devices[i]->serial() == dev->serial()) {
				m_available_devices.erase(m_available_devices.begin() + i);
				return 0;
			}
		}
	}
	return -1;
}

int Session::scan()
{
	int device_count = 0;

	m_lock_devlist.lock();
	m_available_devices.clear();
	m_lock_devlist.unlock();
	struct libusb_device **usb_devs;
	device_count = libusb_get_device_list(m_usb_ctx, &usb_devs);
	if (device_count < 0)
		return -libusb_to_errno(device_count);

	// Iterate over the attached USB devices on the system, adding supported
	// devices to the available list.
	for (int i = 0; i < device_count; i++) {
		Device* dev = probe_device(usb_devs[i]);
		if (dev) {
			m_lock_devlist.lock();
			m_available_devices.push_back(dev);
			m_lock_devlist.unlock();
		}
	}

	libusb_free_device_list(usb_devs, 1);
	return 0;
}

Device* Session::probe_device(libusb_device* usb_dev)
{
	Device* dev = find_existing_device(usb_dev);

	libusb_device_descriptor usb_desc;
	int r = libusb_get_device_descriptor(usb_dev, &usb_desc);
	if (r != 0) {
		DEBUG("Error %i in get_device_descriptor\n", r);
		return NULL;
	}

	std::vector<uint16_t> device_id = {usb_desc.idVendor, usb_desc.idProduct};
	if (std::find(SUPPORTED_DEVICES.begin(), SUPPORTED_DEVICES.end(), device_id)
			!= SUPPORTED_DEVICES.end()) {
		dev = new M1000_Device(this, usb_dev);
	}

	if (dev) {
		if (libusb_open(dev->m_device, &dev->m_usb) == 0) {
			// read serial number, hardware/firmware versions from device
			libusb_get_string_descriptor_ascii(dev->m_usb, usb_desc.iSerialNumber, (unsigned char*)&dev->m_serial, 32);
			dev->ctrl_transfer(0xC0, 0x00, 0, 0, (unsigned char*)&dev->m_hw_version, 64, 100);
			dev->ctrl_transfer(0xC0, 0x00, 0, 1, (unsigned char*)&dev->m_fw_version, 64, 100);
			return dev;
		}
	}
	return NULL;
}

Device* Session::find_existing_device(libusb_device* usb_dev)
{
	std::lock_guard<std::mutex> lock(m_lock_devlist);
	for (Device* dev: m_available_devices) {
		if (dev->m_device == usb_dev) {
			return dev;
		}
	}
	return NULL;
}

Device* Session::get_device(const char* serial)
{
	for (Device* dev: m_devices) {
		if (strncmp(dev->serial(), serial, 31) == 0) {
			return dev;
		}
	}
	return NULL;
}

Device* Session::add(Device* device)
{
	if (device) {
		m_devices.insert(device);
		DEBUG("device insert: %s\n", device->serial());
		device->claim();
		return device;
	}
	return NULL;
}

int Session::add_all()
{
	int ret;

	ret = scan();
	if (ret)
		return ret;

	std::lock_guard<std::mutex> lock(m_lock_devlist);
	for (Device* dev: m_available_devices) {
		if (!add(dev))
			ret++;
	}
	return ret;
}

int Session::remove(Device* device)
{
	int ret = -1;

	if (device) {
		ret = device->release();
		if (!ret)
			m_devices.erase(device);
	}
	return ret;
}

int Session::configure(uint64_t sampleRate)
{
	int ret;

	for (Device* dev: m_devices) {
		ret = dev->configure(sampleRate);
		if (ret)
			break;
	}
	return ret;
}

int Session::run(uint64_t samples)
{
	int ret;

	ret = start(samples);
	if (ret)
		return ret;
	ret = end();
	return ret;
}

int Session::end()
{
	int ret;
	std::unique_lock<std::mutex> lk(m_lock);

	auto now = std::chrono::system_clock::now();
	auto res = m_completion.wait_until(lk, now + std::chrono::seconds(1), [&]{ return m_active_devices == 0; });
	if (!res) {
		DEBUG("timed out\n");
	}

	for (Device* dev: m_devices) {
		ret = dev->off();
		if (ret)
			break;
	}
	return ret;
}

void Session::wait_for_completion()
{
	std::unique_lock<std::mutex> lk(m_lock);
	m_completion.wait(lk, [&]{ return m_active_devices == 0; });
}

int Session::start(uint64_t samples)
{
	int ret;
	m_cancellation = 0;

	for (Device* dev: m_devices) {
		ret = dev->on();
		if (ret)
			break;
		// make sure all devices are synchronized
		if (m_devices.size() > 1) {
			ret = dev->sync();
			if (ret)
				break;
		}
		ret = dev->run(samples);
		if (ret)
			break;
		m_active_devices++;
	}
	return ret;
}

int Session::cancel()
{
	int ret;

	m_cancellation = LIBUSB_TRANSFER_CANCELLED;
	for (Device* dev: m_devices) {
		ret = dev->cancel();
		if (ret)
			break;
	}
	return ret;
}

void Session::handle_error(int status, const char * tag)
{
	std::lock_guard<std::mutex> lock(m_lock);
	// a canceled transfer completing is not an error...
	if ((m_cancellation == 0) && (status != LIBUSB_TRANSFER_CANCELLED) ) {
		DEBUG("error condition at %s: %s\n", tag, libusb_error_name(status));
		m_cancellation = status;
		cancel();
	}
}

void Session::completion()
{
	// On USB thread
	m_active_devices -= 1;
	std::lock_guard<std::mutex> lock(m_lock);
	if (m_active_devices == 0) {
		if (m_completion_callback) {
			m_completion_callback(m_cancellation);
		}
		m_completion.notify_all();
	}
}
