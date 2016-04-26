// Released under the terms of the BSD License
// (C) 2014-2016
//   Analog Devices, Inc.
//   Kevin Mehall <km@kevinmehall.net>
//   Ian Daniher <itdaniher@gmail.com>

#pragma once

#include <libsmu/signal.hpp>
#include <libsmu/session.hpp>

#include <cstdint>
#include <mutex>
#include <vector>

#include <libusb.h>

namespace smu {
	class Device {
	public:
		virtual ~Device();

		/// Get the descriptor for the device.
		/// Pointed-to memory is valid for the lifetime of the Device.
		/// This method may be called on a device that is not added to the session.
		virtual const sl_device_info* info() const = 0;

		/// Get the descriptor for the specified channel.
		/// Pointed-to memory is valid for the lifetime of the Device.
		virtual const sl_channel_info* channel_info(unsigned channel) const = 0;

		/// Get the specified Signal.
		virtual Signal* signal(unsigned channel, unsigned signal) = 0;

		/// Get the serial number of the device.
		/// Pointed-to memory is valid for the lifetime of the Device.
		/// This method may be called on a device that is not added to the session.
		virtual const char* serial() const { return this->serial_num; }
		virtual const char* fwver() const { return this->m_fw_version; }
		virtual const char* hwver() const { return this->m_hw_version; }

		/// Set the mode of the specified channel.
		/// This method may not be called while the session is active.
		virtual void set_mode(unsigned channel, unsigned mode) = 0;

		/// Perform a raw USB control transfer on the underlying USB device
		int ctrl_transfer(unsigned bmRequestType, unsigned bRequest, unsigned wValue, unsigned wIndex,
						unsigned char *data, unsigned wLength, unsigned timeout);

		/// Force the device into SAM-BA command mode.
		virtual void samba_mode() = 0;

		/// Get the default sample rate.
		virtual int get_default_rate() { return 10000; }

		/// Prepare multi-device synchronization.
		virtual void sync() = 0;

		/// Lock the Device's mutex, preventing this device's transfers from being processed. Hold
		/// this lock only briefly, while modifying Signal state.
		virtual void lock() { m_state.lock(); }

		/// Unlock the Device's mutex, allowing this device's transfers to be processed.
		virtual void unlock() { m_state.unlock(); }

		/// Write the device calibration data into the EEPROM.
		virtual int write_calibration(const char* cal_file_name) { return 0; }

		/// Get the device calibration data from the EEPROM.
		virtual void calibration(std::vector<std::vector<float>>* cal) = 0;

	protected:
		Device(Session* s, libusb_device* d);
		virtual int init();
		virtual int added() { return 0; }
		virtual int removed() { return 0; }
		virtual void configure(uint64_t sampleRate) = 0;

		virtual void on() = 0;
		virtual void off() = 0;
		virtual void start_run(uint64_t nsamples) = 0;
		virtual void cancel() = 0;

		Session* const m_session;
		libusb_device* const m_device = NULL;
		libusb_device_handle* m_usb = NULL;

		// State owned by USB thread
		uint64_t m_requested_sampleno = 0;
		uint64_t m_in_sampleno = 0;
		uint64_t m_out_sampleno = 0;

		std::mutex m_state;

		char m_fw_version[32];
		char m_hw_version[32];
		char serial_num[32];

		friend class Session;
	};
}
