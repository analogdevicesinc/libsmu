// Released under the terms of the BSD License
// (C) 2014-2016
//   Analog Devices, Inc.
//   Kevin Mehall <km@kevinmehall.net>
//   Ian Daniher <itdaniher@gmail.com>

#include <ctime>
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

extern std::exception_ptr e_ptr;

using namespace std::placeholders;  // for _1, _2, _3...
using namespace smu;

Session::Session()
{
	m_active_devices = 0;
	int ret;

	ret = libusb_init(&m_usb_ctx);
	if (ret != 0) {
		DEBUG("%s: libusb init failed: %s\n", __func__, libusb_error_name(ret));
		abort();
	}

	struct timeval zero_tv;
	zero_tv.tv_sec = 0;
	zero_tv.tv_usec = 1;

	// Spawn a thread to handle pending USB events.
	m_usb_thread_loop = true;
	m_usb_thread = std::thread([=]() {
		while (m_usb_thread_loop) {
			libusb_handle_events_timeout_completed(m_usb_ctx, const_cast<timeval *>(&zero_tv), NULL);
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

	// Cancel all outstanding transfers.
	cancel();

	// Run device destructors before libusb_exit().
	for (Device* dev: m_devices) {
		// reset devices to high impedance mode before removing
		dev->set_mode(0, HI_Z);
		dev->set_mode(1, HI_Z);
		delete dev;
	}

	m_devices.clear();
	m_available_devices.clear();

	// Stop USB thread loop. This must be called right before libusb_exit() so
	// all USB events, including closing devices, are handled properly. Certain
	// events (such as those triggered by libusb_close()) can cause hangs
	// within libusb if called after event handling is stopped.
	if (m_usb_thread.joinable()) {
		m_usb_thread_loop = false;
		m_usb_thread.join();
	}

	libusb_exit(m_usb_ctx);
}

void Session::set_off(Device* dev)
{
	M1000_Device *m_dev = dynamic_cast<M1000_Device*>(dev);
	m_dev->m_state.unlock();
}

void Session::attached(libusb_device *usb_dev)
{
	if (!m_hotplug_attach_callbacks.empty()) {
		Device* dev = probe_device(usb_dev);
		if (dev) {
			std::lock_guard<std::mutex> lock(m_lock_devlist);
			m_available_devices.push_back(dev);
			for (auto callback: m_hotplug_attach_callbacks) {
				// Store exceptions to rethrow them in the main thread in read()/write().
				try {
					callback(dev);
				} catch (...) {
					e_ptr = std::current_exception();
				}
			}
		}
	}
}

void Session::detached(libusb_device *usb_dev)
{
	if (!m_hotplug_detach_callbacks.empty()) {
		Device* dev = find_existing_device(usb_dev);
		if (dev) {
			for (auto callback: m_hotplug_detach_callbacks) {
				// Store exceptions to rethrow them in the main thread in read()/write().
				try {
					callback(dev);
				} catch (...) {
					e_ptr = std::current_exception();
				}
			}
		}
	}
}

// Internal function to write raw SAM-BA commands to a libusb handle.
static void samba_usb_write(libusb_device_handle *usb_handle, const char* data) {
	int transferred, ret;
	ret = libusb_bulk_transfer(usb_handle, 0x01, (unsigned char *)data, strlen(data), &transferred, 100);
	if (ret < 0) {
		std::string libusb_error_str(libusb_strerror((enum libusb_error)ret));
		throw std::runtime_error("failed to write SAM-BA command: " + libusb_error_str);
	}
}

// Internal function to read raw SAM-BA commands to a libusb handle.
static void samba_usb_read(libusb_device_handle *usb_handle, unsigned char* data) {
	int transferred, ret;
	ret = libusb_bulk_transfer(usb_handle, 0x82, data, 512, &transferred, 100);
	if (ret < 0) {
		std::string libusb_error_str(libusb_strerror((enum libusb_error)ret));
		throw std::runtime_error("failed to read SAM-BA response: " + libusb_error_str);
	}
}

int Session::scan_samba_devs(std::vector<libusb_device*>& samba_devs)
{
	unsigned int device_count;
	libusb_device **usb_devs;
	struct libusb_device_descriptor usb_info;
	std::vector<uint16_t> samba_device_id;

	device_count = libusb_get_device_list(m_usb_ctx, &usb_devs);
	if (device_count < 0)
		return -libusb_to_errno(device_count);

	// Walk the list of USB devices looking for devices in SAM-BA mode.
	for (unsigned int i = 0; i < device_count; i++) {
		libusb_get_device_descriptor(usb_devs[i], &usb_info);
		samba_device_id = {usb_info.idVendor, usb_info.idProduct};
		if (std::find(SAMBA_DEVICES.begin(), SAMBA_DEVICES.end(), samba_device_id)
				!= SAMBA_DEVICES.end()) {
			samba_devs.push_back(usb_devs[i]);
		}
	}

	return samba_devs.size();
}

int Session::flash_firmware(std::string file, std::vector<Device*> devices)
{
	int device_count = 0;
	std::unique_lock<std::mutex> lock(m_lock_devlist);

	// if no devices are specified, flash all supported devices on the system
	if (devices.size() == 0)
		devices = m_available_devices;

	std::ifstream firmware (file, std::ios::in | std::ios::binary);
	long firmware_size;

	if (!firmware.is_open()) {
		throw std::runtime_error("failed to open firmware file");
	}

	// TODO: verify that file is a compatible firmware file
	// read firmware file into buffer
	firmware.seekg(0, std::ios::end);
	firmware_size = firmware.tellg();
	firmware_size = firmware_size + (256 - firmware_size % 256);
	firmware.seekg(0, std::ios::beg);
	auto fwdata = std::unique_ptr<char[]>{ new char[firmware_size] };
	firmware.read(fwdata.get(), firmware_size);
	firmware.close();

	auto flash_device = [&](libusb_device *usb_dev) {
		libusb_device_handle *usb_handle = NULL;
		unsigned char usb_data[512];
		unsigned int page;
		const uint32_t flashbase = 0x80000;
		int ret;

		ret = libusb_open(usb_dev, &usb_handle);
		if (ret < 0) {
			std::string libusb_error_str(libusb_strerror((enum libusb_error)ret));
			throw std::runtime_error("failed opening USB device: " + libusb_error_str);
		}
#ifndef _WIN32
		libusb_detach_kernel_driver(usb_handle, 0);
		libusb_detach_kernel_driver(usb_handle, 1);
#endif
		libusb_claim_interface(usb_handle, 1);

		// ease of use abbreviations
		auto samba_write = std::bind(samba_usb_write, usb_handle, _1);
		auto samba_read = std::bind(samba_usb_read, usb_handle, usb_data);

		// erase flash
		samba_write("W400E0804,5A000005#");
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		samba_read();
		// check if flash is erased
		samba_write("w400E0808,4#");
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		samba_read();
		samba_read();
		samba_read();

		// write firmware
		page = 0;
		char cmd[20];
		uint32_t data;
		for (auto pos = 0; pos < firmware_size; pos += 4) {
			data = (uint8_t)fwdata[pos] | (uint8_t)fwdata[pos+1] << 8 |
				(uint8_t)fwdata[pos+2] << 16 | (uint8_t)fwdata[pos+3] << 24;
			snprintf(cmd, sizeof(cmd), "W%.8X,%.8X#", flashbase + pos, data);
			samba_write(cmd);
			samba_read();
			samba_read();
			// On page boundaries, write the page.
			if ((pos & 0xFC) == 0xFC) {
				snprintf(cmd, sizeof(cmd), "W400E0804,5A00%.2X03#", page);
				samba_write(cmd);
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				samba_read();
				samba_read();
				// Verify page is written.
				samba_write("w400E0808,4#");
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				samba_read();
				samba_read();
				samba_read();
				// TODO: check page status
				page++;
			}
		}

		// TODO: verify flashed data

		// disable SAM-BA
		samba_write("W400E0804,5A00010B#");
		samba_read();
		samba_read();
		// jump to flash
		samba_write("G00000000#");
		samba_read();

		libusb_release_interface(usb_handle, 1);
		libusb_close(usb_handle);
	};

	// Removing devices and putting them into SAM-BA mode triggers hotplug
	// routines which can cause the m_lock_devlist to be reacquired if any
	// hotplug callbacks exist.
	lock.unlock();

	// force all specified devices into SAM-BA mode
	// TODO: revert to unsigned index when VS supports OpenMP 3.0
	#pragma omp parallel for
	for (int i = 0; i < (int)devices.size(); i++) {
		Device* dev = devices[i];
		if (dev) {
			#pragma omp critical
			{
				remove(dev);
			}
			dev->samba_mode();
			delete dev;
		}
	}

	std::vector<libusb_device*> samba_devs;
	device_count = scan_samba_devs(samba_devs);
	if (device_count < 0)
		throw std::runtime_error("failed to scan for devices in SAM-BA mode");
	else if (device_count == 0)
		throw std::runtime_error("no devices found in SAM-BA mode");
	else if (device_count < (int)devices.size())
		throw std::runtime_error("failed forcing devices into SAM-BA mode");

	// flash all devices in SAM-BA mode
	#pragma omp parallel for
	for (int i = 0; i < device_count; i++) {
		try {
			flash_device(samba_devs[i]);
		} catch (...) {
			e_ptr = std::current_exception();
		}
	}

	if (e_ptr) {
		// copy exception pointer for throwing and reset it
		std::exception_ptr new_e_ptr = e_ptr;
		e_ptr = nullptr;
		std::rethrow_exception(new_e_ptr);
	}

	return device_count;
}

int Session::destroy(Device *dev)
{
	// This method may not be called while the session is active.
	if (m_active_devices)
		return -EBUSY;

	std::lock_guard<std::mutex> lock(m_lock_devlist);
	if (dev) {
		for (unsigned i = 0; i < m_available_devices.size(); i++) {
			if (m_available_devices[i]->m_serial.compare(dev->m_serial) == 0) {
				m_available_devices.erase(m_available_devices.begin() + i);
				return 0;
			}
		}
	}
	return -ENODEV;
}

int Session::scan()
{
	int device_count = 0;
	int devices_found = 0;

	m_lock_devlist.lock();
	m_available_devices.clear();
	m_lock_devlist.unlock();
	libusb_device **usb_devs;
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
			devices_found++;
		}
	}

	libusb_free_device_list(usb_devs, 1);
	return devices_found;
}

Device* Session::probe_device(libusb_device* usb_dev)
{
	int ret;
	Device* dev = find_existing_device(usb_dev);

	libusb_device_descriptor usb_desc;
	ret = libusb_get_device_descriptor(usb_dev, &usb_desc);
	if (ret != 0) {
		DEBUG("%s: error %i in get_device_descriptor\n", __func__, ret);
		return NULL;
	}

	// check if device is supported
	std::vector<uint16_t> device_id = {usb_desc.idVendor, usb_desc.idProduct};
	if (std::find(SUPPORTED_DEVICES.begin(), SUPPORTED_DEVICES.end(), device_id)
			!= SUPPORTED_DEVICES.end()) {
		libusb_device_handle *usb_handle = NULL;

		/* TO DO:
		 *		This is a workaround for the bug from libusb (libusb_open returns -3 when opening the device's interface for the second time).
		 *		Instead of requesting another interface on each search, we save the device's first given libusb interface in m_deviceHandles and use it every time when
		 *		the function returns -3.
		 *
		 *		Check this when newer libusb versions are released.
		 */


		uint8_t addr = libusb_get_device_address(usb_dev);
		uint8_t bus = libusb_get_bus_number(usb_dev);
		std::pair<uint8_t, uint8_t> usb_id_addr(bus, addr);

		int open_errorcode = libusb_open(usb_dev, &usb_handle);

		// probably lacking permission to open the underlying usb device
		if (open_errorcode != 0) {
			if (open_errorcode != -3) {
				return NULL;
			} else {
				usb_handle = m_deviceHandles[usb_dev];
			}
		}

		for (auto d : m_devices) {
			if ((d->m_usb_addr.first == usb_id_addr.first) &&
				(d->m_usb_addr.second == usb_id_addr.second)) {
				return d;
			}
		}

		m_deviceHandles[usb_dev] = usb_handle;

		char serial[32] = "";
		char fwver[32] = "";
		char hwver[32] = "";

		// serial/hw/fw versions should exist otherwise the USB cable probably has issues
		ret = libusb_get_string_descriptor_ascii(usb_handle, usb_desc.iSerialNumber, (unsigned char*)&serial, 32);
		if (ret <= 0 || (strncmp(serial, "", 1) == 0))
			return NULL;
		ret = libusb_control_transfer(usb_handle, 0xC0, 0x00, 0, 0, (unsigned char*)&hwver, 64, 100);
		if (ret <= 0 || (strncmp(hwver, "", 1) == 0))
			return NULL;
		ret = libusb_control_transfer(usb_handle, 0xC0, 0x00, 0, 1, (unsigned char*)&fwver, 64, 100);
		if (ret <= 0 || (strncmp(fwver, "", 1) == 0))
			return NULL;

		dev = new M1000_Device(this, usb_dev, usb_handle, hwver, fwver, serial);
		dev->set_usb_device_addr(usb_id_addr);
		dev->read_calibration();
		return dev;
	}
	return NULL;
}

Device* Session::find_existing_device(libusb_device* usb_dev)
{
	std::lock_guard<std::mutex> lock(m_lock_devlist);
	for (Device* dev: m_available_devices) {
		if (dev->m_usb_dev == usb_dev) {
			return dev;
		}
	}
	return NULL;
}

int Session::add(Device* device)
{
	int ret = -1;

	// This method may not be called while the session is active.
	if (m_active_devices)
		return -EBUSY;

	if (device) {
		ret = device->claim();
		if (!ret)
		{
			for (auto it : m_devices)
			{
				if (it->m_serial.compare(device->m_serial) == 0)
				{
					m_devices.erase(it);
					break;
				}
			}

			m_devices.insert(device);
		}
	}

	return ret;
}

int Session::add_all()
{
	int ret;
	int num_devices = 0;

	// This method may not be called while the session is active.
	if (m_active_devices)
		return -EBUSY;

	ret = scan();
	if (ret < 0)
		return ret;

	std::lock_guard<std::mutex> lock(m_lock_devlist);
	for (Device* dev: m_available_devices) {
		ret = add(dev);
		if (ret)
			break;
		num_devices++;
	}

	if (ret < 0)
		return ret;
	return num_devices;
}

int Session::remove(Device* device, bool detached)
{
	int ret = -1;

	// This method may not be called while the session is active.
	if (m_active_devices)
		return -EBUSY;

	if (device) {
		ret = device->release();

		// device has already been detached from the system
		if (detached && (ret == -19))
			ret = 0;

		if (!ret)
			m_devices.erase(device);
	}
	return ret;
}

int Session::configure(uint32_t sampleRate)
{
	int ret = 0;

	// This method may not be called while the session is active.
	if (m_active_devices)
		return -EBUSY;

	// Nothing to configure if the session has no devices.
	if (m_devices.size() == 0)
		return ret;

	// Passing a sample rate of 0 defaults to the initial device's default
	// sample rate.
	if (sampleRate == 0) {
		Device* dev = *(m_devices.begin());
		sampleRate = dev->get_default_rate();
	}

	for (Device* dev: m_devices) {
		ret = dev->configure(sampleRate);
		if (ret < 0)
			break;
	}

	if (ret > 0)
		m_sample_rate = ret;

	return ret;
}

int Session::run(uint64_t samples)
{
	int ret;

	if (samples > 0 && m_continuous) {
		// running session in noncontinuous mode while already running in
		// continuous mode doesn't work
		return -EBUSY;
	}

	m_samples = samples;
	ret = start(samples);
	if (ret)
		return ret;
	ret = end();
	return ret;
}

int Session::end()
{
	int ret = 0;
	// cancel continuous sessions before ending them
	if (m_continuous) {
		cancel();
		m_continuous = false;
	}

	// Wait up to a second for devices to finish streaming.
	std::unique_lock<std::mutex> lk(m_lock);
	auto now = std::chrono::system_clock::now();
	uint64_t waitTime;
	if(m_sample_rate != 0){
		waitTime = (m_samples/m_sample_rate + 1) + 1;
	}
	else{
		waitTime = 0;
	}

	auto res = m_completion.wait_until(lk, now + std::chrono::seconds(waitTime), [&]{ return m_active_devices == 0; });
	if (!res) {
		DEBUG("%s: timed out waiting for completion\n", __func__);
	}

	for (Device* dev: m_devices) {
		ret = dev->off();
		if (ret == -ENODEV) {
			// the device has already been detached
			ret = 0;
			continue;
		} else if (ret) {
			break;
		}
	}
	return ret;
}

void Session::flush()
{
	for (Device* dev: m_devices) {
		// flush all read/write queues
		dev->flush(0, true);
		dev->flush(1, true);
	}
}

int Session::start(uint64_t samples)
{
	int ret = 0;
	m_cancellation = 0;
	m_continuous = (samples == 0);

	// if session is unconfigured, use device default sample rate
	if (m_sample_rate == 0)
		configure(0);

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
	int ret = 0;

	m_cancellation = LIBUSB_TRANSFER_CANCELLED;
	for (Device* dev: m_devices) {
		set_off(dev);
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
		DEBUG("%s: error condition at %s: %s\n", __func__, tag, libusb_error_name(status));
		m_cancellation = status;
		cancel();
	}
}

void Session::completion()
{
	// on USB thread
	m_active_devices -= 1;

	// don't lock for cancelled sessions
	if (m_cancellation == 0)
		std::unique_lock<std::mutex> lock(m_lock);

	if (m_active_devices == 0) {
		if (m_completion_callback) {
			m_completion_callback(m_cancellation);
		}
		m_completion.notify_all();
	}
}
