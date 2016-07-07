// Released under the terms of the BSD License
// (C) 2014-2016
//   Analog Devices, Inc.
//   Kevin Mehall <km@kevinmehall.net>
//   Ian Daniher <itdaniher@gmail.com>

/// @file libsmu.hpp
/// @brief Main public interface.

#pragma once

#include <cstdint>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

#include <libusb.h>

#include <libsmu/version.hpp>

/// @brief List of supported devices.
/// The list uses the vendor and project IDs from USB
/// information formatted as {vendor_id, product_id}.
const std::vector<std::vector<uint16_t>> SUPPORTED_DEVICES = {
	{0x0456, 0xcee2},
	{0x064b, 0x784c},
};

/// @brief List of supported devices in SAM-BA bootloader mode.
/// The list uses the vendor and project IDs from USB information
/// formatted as {vendor_id, product_id}.
const std::vector<std::vector<uint16_t>> SAMBA_DEVICES = {
	{0x03eb, 0x6124},
};

/// @brief Signal information.
typedef struct sl_signal_info {
	/// Signal label.
	const char* label;

	/// Bitmask of modes for which this signal is enabled as input.
	uint32_t inputModes;

	/// Bitmask of modes for which this signal is enabled as output.
	uint32_t outputModes;

	/// Minimum possible value for the signal.
	double min;

	/// Maximum possible value for the signal.
	double max;

	/// Signal resolution.
	double resolution;
} sl_signal_info;

/// @brief Channel info.
typedef struct sl_channel_info {
	const char* label; ///< Channel label.
	size_t mode_count; ///< Number of available modes.
	size_t signal_count; ///< Number of available signals.
} sl_channel_info;

/// @brief Device info.
typedef struct sl_device_info {
	const char* label; ///< Device label.
	size_t channel_count; ///< Number of available channels.
} sl_device_info;

/// @brief Supported signal sources.
enum Src {
	SRC_CONSTANT, ///< Constant value output. 
	SRC_SQUARE, ///< Square wave output.
	SRC_SAWTOOTH, ///< Sawtooth wave output.
	SRC_STAIRSTEP, ///< Stairstep wave output.
	SRC_SINE, ///< Sine wave output.
	SRC_TRIANGLE, ///< Triangle wave output.
	SRC_BUFFER, ///< Use samples from a specified buffer.
	SRC_CALLBACK, ///< Use samples from a specified callback function.
};

/// @brief Supported signal destinations.
enum Dest {
	DEST_NONE, ///< Samples are discarded.
	DEST_BUFFER, ///< Samples are buffered into a specified location.
	DEST_CALLBACK, ///< Samples are passed to a specified callback function.
};

/// @brief Supported channel modes.
enum Modes {
	DISABLED, ///< Channel is disabled.
	SVMI, ///< Source voltage, measure current.
	SIMV, ///< Source current, measure voltage.
};

namespace smu {
	class Device;
	class Signal;

	/// @brief Generic session class.
	class Session {
	public:
		Session();
		~Session();

		/// @brief Scan system for all supported devices.
		/// @return On success, 0 is returned.
		/// @return On error, a negative integer relating to a libusb error code is returned.
		int update_available_devices();

		/// @brief Devices that are present on the system.
		/// Note that these devices aren't necessarily bound to a session.
		std::vector<std::shared_ptr<Device>> m_available_devices;

		/// @brief Add a device to the session.
		/// This method may not be called while the session is active.
		/// @param device The Device to be added.
		/// @return On success, the added device is returned.
		/// @return On error, NULL is returned.
		Device* add_device(Device* device);

		/// @brief Devices that are part of this session.
		/// These devices will be started when start() is called.
		/// Use `add_device` and `remove_device` to manipulate this list.
		std::set<Device*> m_devices;

		/// @brief Number of devices currently streaming samples.
		unsigned m_active_devices;

		/// @brief Get the device matching a given serial from the session.
		/// @param serial A string of a device's serial number.
		/// @return On success, the matching Device is returned.
		/// @return If no match is found, NULL is returned.
		Device* get_device(const char* serial);

		/// @brief Remove a device from the session.
		/// @param device A Device to be removed.
		/// This method may not be called while the session is active.
		void remove_device(Device* device);

		/// @brief Remove a device from the list of available devices.
		/// @param device A Device to be removed from the available list.
		/// Devices are automatically added to this list on attach.
		/// Devices must be removed from this list on detach.
		/// This method may not be called while the session is active
		void destroy_available(Device* device);

		/// @brief Configure the session's sample rate.
		/// @param sampleRate The requested sample rate for the session.
		/// This method may not be called while the session is active.
		void configure(uint64_t sampleRate);

		/// @brief Run the currently configured capture and wait for it to complete.
		/// @param nsamples Number of samples to capture until we stop. If 0, run in continuous mode.
		void run(uint64_t nsamples);

		/// @brief Start the currently configured capture, but do not wait for it to complete.
		/// @param nsamples Number of samples to capture until we stop. If 0, run in continuous mode.
		/// Once started, the only allowed Session methods are cancel() and end()
		/// until the session has stopped.
		void start(uint64_t nsamples);

		/// @brief Cancel capture and block waiting for it to complete.
		void cancel();

		/// @brief Update device firmware for a given device.
		/// @param file Firmware file started for deployment to the device.
		/// @param device The Device targeted for updating.
		/// If device is NULL the first attached device in a session will be
		/// used instead. If no configured devices are found, devices in SAM-BA
		/// bootloader mode are searched for and the first matching device is used.
		/// @throws std::runtime_error for various USB failures causing aborted flashes.
		void flash_firmware(const char *file, Device* device = NULL);

		/// internal: Called by devices on the USB thread when they are complete.
		void completion();

		/// internal: Called by devices on the USB thread when a device encounters an error.
		void handle_error(int status, const char * tag);

		/// internal: Called by devices on the USB thread with progress updates.
		void progress();

		/// internal: Called by device attach events on the USB thread.
		void attached(libusb_device* device);
		/// internal: Called by device detach events on the USB thread.
		void detached(libusb_device* device);

		/// @brief Block until all devices have completed.
		void wait_for_completion();

		/// @brief Block until all devices have completed, then turn off the devices.
		void end();

		/// @brief Callback called on the USB thread with the sample number as samples are received.
		std::function<void(uint64_t)> m_progress_callback;

		/// @brief Callback called on the USB thread on completion.
		std::function<void(unsigned)> m_completion_callback;

		/// @brief Callback called on the USB thread when a device is plugged into the system.
		std::function<void(Device* device)> m_hotplug_detach_callback;

		/// @brief Callback called on the USB thread when a device is removed from the system.
		std::function<void(Device* device)> m_hotplug_attach_callback;

		/// @brief Flag used to cancel all pending USB transactions for devices in a session.
		unsigned m_cancellation = 0;

	protected:
		/// @brief Flag for TODO
		uint64_t m_min_progress = 0;

		/// @brief Spawn thread for USB transaction handling.
		void start_usb_thread();
		/// @brief Flag for TODO
		std::thread m_usb_thread;
		/// @brief Flag for TODO
		bool m_usb_thread_loop;

		/// @brief Flag for TODO
		std::mutex m_lock;
		/// @brief Flag for TODO
		std::mutex m_lock_devlist;
		/// @brief Flag for TODO
		std::condition_variable m_completion;

		/// @brief libusb context related with a session. This allows for segregating
		/// libusb usage so external users can also use libusb without interfering
		/// with internal usage.
		libusb_context* m_usb_cx;

		/// @brief Identify devices supported by libsmu.
		/// @param device a libusb device handle
		/// @return If the usb device relates
		/// to a supported device the Device is returned,
		/// otherwise NULL is returned.
		std::shared_ptr<Device> probe_device(libusb_device* device);

		/// @brief Find an existing, available device.
		/// @param device a libusb device handle
		/// @return If the usb device relates to an existing,
		/// available device the Device is returned,
		/// otherwise NULL is returned.
		std::shared_ptr<Device> find_existing_device(libusb_device* device);
	};

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
		/// @param signal An unsigned integer relating to the requested signal.
		/// @return The related Signal.
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
		/// @return Passes through the return value of the underlying libusb_control_transfer method.
		int ctrl_transfer(unsigned bmRequestType, unsigned bRequest, unsigned wValue, unsigned wIndex,
						unsigned char *data, unsigned wLength, unsigned timeout);

		/// @brief Force the device into SAM-BA bootloader mode.
		virtual void samba_mode() = 0;

		/// @brief Get the default sample rate.
		virtual int get_default_rate() { return 10000; }

		/// @brief Prepare multi-device synchronization.
		/// Get current microframe index, set m_sof_start to be time in the future.
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
		/// @param cal A vector of float values.
		virtual void calibration(std::vector<std::vector<float>>* cal) = 0;

	protected:
		/// @brief Device constructor.
		Device(Session* s, libusb_device* d);

		/// @brief Device claiming and initialization when a session adds this device.
		virtual int added() { return 0; }

		/// @brief Device releasing when a session removes this device.
		virtual int removed() { return 0; }

		/// @brief Configurization and initialization for device sampling.
		/// @param sampleRate The requested sampling rate for the device.
		virtual void configure(uint64_t sampleRate) = 0;

		/// @brief Turn on power supplies and clear sampling state.
		virtual void on() = 0;

		/// @brief Stop sampling and put outputs into high-impedance mode.
		virtual void off() = 0;

		/// @brief Make the device start sampling.
		virtual void start_run(uint64_t nsamples) = 0;

		/// @brief Cancel all pending libusb transactions.
		virtual void cancel() = 0;

		/// @brief Session this device is associated with.
		Session* const m_session;

		/// @brief Underlying libusb device.
		libusb_device* const m_device = NULL;
		/// @brief Underlying libusb device handle.
		libusb_device_handle* m_usb = NULL;

		// State owned by USB thread
		uint64_t m_requested_sampleno = 0;
		uint64_t m_in_sampleno = 0;
		uint64_t m_out_sampleno = 0;

		std::mutex m_state;

		/// firmware version
		char m_fw_version[32];
		/// hardware version
		char m_hw_version[32];
		/// serial number
		char serial_num[32];

		friend class Session;
	};

	/// @brief Generic signal class.
	class Signal {
	public:
		/// internal: Do not call the constructor directly; obtain a Signal from a Device.
		Signal(const sl_signal_info* info):
			m_info(info),
			m_src(SRC_CONSTANT),
			m_src_v1(0),
			m_dest(DEST_NONE)
			{}

		/// Signal destructor.
		~Signal();

		/// @brief Get the descriptor struct of the Signal.
		/// Pointed-to memory is valid for the lifetime of the Device.
		const sl_signal_info* info() const { return m_info; }
		const sl_signal_info* const m_info;

		void source_constant(float val);

		void source_square(float midpoint, float peak, double period, double duty, double phase);

		void source_sawtooth(float midpoint, float peak, double period, double phase);

		void source_stairstep(float midpoint, float peak, double period, double phase);

		void source_sine(float midpoint, float peak, double period, double phase);

		void source_triangle(float midpoint, float peak, double period, double phase);

		void source_buffer(float* buf, size_t len, bool repeat);

		void source_callback(std::function<float (uint64_t index)> callback);

		/// Get the last measured sample from this signal.
		float measure_instantaneous() { return m_latest_measurement; }

		/// @brief Store received samples in a buffer.
		/// @param buf Buffer to use for sample storage.
		/// @param len Number of samples to store.
		/// Samples are dropped once the number of samples received surpasses the
		/// configured storage length.
		void measure_buffer(float* buf, size_t len);

		/// @brief Configure received samples to be passed to the provided callback.
		/// @param callback Callback method to operate on sample stream float values.
		void measure_callback(std::function<void(float value)> callback);

		/// internal: Called by Device
		void put_sample(float val);

		/// internal: Called by Device
		float get_sample();

		void update_phase(double new_period, double new_phase);

		Src m_src;
		float m_src_v1;
		float m_src_v2;
		double m_src_period;
		double m_src_duty;
		double m_src_phase;

		float* m_src_buf = NULL;
		size_t m_src_i;
		size_t m_src_buf_len;
		bool m_src_buf_repeat;

		std::function<float (uint64_t index)> m_src_callback;

		Dest m_dest;

		// valid if m_dest == DEST_BUF
		float* m_dest_buf;
		size_t m_dest_buf_len;

		// valid if m_dest == DEST_CALLBACK
		std::function<void(float val)> m_dest_callback;

	protected:
		float m_latest_measurement;
	};
}
