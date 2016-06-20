// Released under the terms of the BSD License
// (C) 2014-2016
//   Analog Devices, Inc.
//   Kevin Mehall <km@kevinmehall.net>
//   Ian Daniher <itdaniher@gmail.com>

/// @file device.hpp
/// @brief Device handling.

#pragma once

#include <libsmu/signal.hpp>
#include <libsmu/session.hpp>

#include <cstdint>
#include <mutex>
#include <vector>

#include <libusb.h>

namespace smu {
	/// @brief Generic device class.
	class Device {
	public:
		virtual ~Device();

		/// @brief Get the descriptor for the device.
		virtual const sl_device_info* info() const = 0;

		/// @brief Get the descriptor for the specified channel.
		/// @param channel An unsigned integer relating to the requested channel.
		virtual const sl_channel_info* channel_info(unsigned channel) const = 0;

		/// @brief Get the specified signal.
		/// @param channel An unsigned integer relating to the requested channel.
		/// @param channel An unsigned integer relating to the requested signal.
		virtual Signal* signal(unsigned channel, unsigned signal) = 0;

		/// @brief Get the serial number of the device.
		virtual const char* serial() const { return this->serial_num; }

		/// @brief Get the firmware version of the device.
		virtual const char* fwver() const { return this->m_fw_version; }

		/// @brief Get the hardware version of the device.
		virtual const char* hwver() const { return this->m_hw_version; }

		/// @brief Set the mode of the specified channel.
		/// @param channel An unsigned integer relating to the requested channel.
		/// @param mode An unsigned integer relating to the requested mode.
		/// This method may not be called while the session is active.
		virtual void set_mode(unsigned channel, unsigned mode) = 0;

		/// @brief Perform a raw USB control transfer on the underlying USB device.
		int ctrl_transfer(unsigned bmRequestType, unsigned bRequest, unsigned wValue, unsigned wIndex,
						unsigned char *data, unsigned wLength, unsigned timeout);

		/// @brief Force the device into SAM-BA command mode.
		virtual void samba_mode() = 0;

		/// @brief Get the default sample rate.
		virtual int get_default_rate() { return 10000; }

		/// @brief Prepare multi-device synchronization.
		virtual void sync() = 0;

		/// @brief Lock the device's mutex.
		/// This prevents this device's transfers from being processed. Hold
		/// only briefly, while modifying signal state.
		virtual void lock() { m_state.lock(); }

		/// @brief Unlock the device's mutex.
		/// Allows this device's transfers to be processed.
		virtual void unlock() { m_state.unlock(); }

		/// @brief Write the device calibration data into the EEPROM.
		/// @param cal_file_name The path to a properly-formatted calibration
		/// data file to write to the device.
		/// @return On success, 0 is returned.
		/// @return On error, a negative integer is returned.
		virtual int write_calibration(const char* cal_file_name) { return 0; }

		/// @brief Get the device calibration data from the EEPROM.
		/// @param cal A pointer to a vector of floats.
		virtual void calibration(std::vector<std::vector<float>>* cal) = 0;

	protected:
		Device(Session* s, libusb_device* d);

		/// @brief Generic device initialization.
		virtual int init();

		/// @brief Device claiming and initialization when a session adds this device.
		virtual int added() { return 0; }

		/// @brief Device releasing when a session removes this device.
		virtual int removed() { return 0; }

		/// @brief Configurization and initialization for device sampling.
		/// @param sampleRate The requests sampling rate for the device.
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
