// Released under the terms of the BSD License
// (C) 2014-2016
//   Analog Devices, Inc.
//   Kevin Mehall <km@kevinmehall.net>
//   Ian Daniher <itdaniher@gmail.com>

#ifndef __LIBSMU_HPP__
#define __LIBSMU_HPP__

#include "version.hpp"

#include <cstdint>
#include <cstddef>
#include <string>
#include <set>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <vector>

#include <libusb.h>

namespace smu {
	class Device;
	class Signal;
}

/// List of supported devices, using vendor and project IDs from USB
/// information formatted as {vendor_id, product_id}.
const std::vector<std::vector<uint16_t>> SUPPORTED_DEVICES = {
	{0x0456, 0xcee2},
	{0x064b, 0x784c},
};

/// List of supported devices in SAM-BA bootloader mode, using vendor and
/// project IDs from USB information formatted as {vendor_id, product_id}.
const std::vector<std::vector<uint16_t>> SAMBA_DEVICES = {
	{0x03eb, 0x6124},
};

typedef enum sl_type {
	DEVICE_M1000 = 0x10000,
	CHANNEL_SMU = 0x20000,
	MODE_HIGH_Z = 0x40000,
	MODE_SVMI,
	MODE_SIMV,
	SIGNAL = 0x80000,
} sl_type;

typedef struct sl_unit {
	int8_t m;
	int8_t kg;
	int8_t s;
	int8_t A;
	int8_t K;
	int8_t mol;
	int8_t cd;
} sl_unit;

const sl_unit unit_V = { 2,  1, -3, -1,  0,  0,  0};
const sl_unit unit_A = { 0,  0,  0,  1,  0,  0,  0};

typedef struct sl_signal_info {
	sl_type type;

	const char* label;

	/// Bitmask of modes for which this signal is enabled as input
	uint32_t inputModes;

	/// Bitmask of modes for which this signal is enabled as output
	uint32_t outputModes;

	sl_unit unit;
	double min;
	double max;
	double resolution;
} sl_signal_info;

typedef struct sl_channel_info {
	sl_type type;
	const char* label;
	size_t mode_count;
	size_t signal_count;
} sl_channel_info;

typedef struct sl_device_info {
	sl_type type;
	const char* label;
	size_t channel_count;
} sl_device_info;

enum Dest {
	DEST_NONE,
	DEST_BUFFER,
	DEST_CALLBACK,
};

enum Src {
	SRC_CONSTANT,
	SRC_SQUARE,
	SRC_SAWTOOTH,
	SRC_STAIRSTEP,
	SRC_SINE,
	SRC_TRIANGLE,
	SRC_BUFFER,
	SRC_CALLBACK,
};

enum Modes {
	DISABLED,
	SVMI,
	SIMV,
};


namespace smu {
	class Session {
	public:
		Session();
		~Session();

		int update_available_devices();
		unsigned m_active_devices;

		/// Devices that are present on the system, but aren't necessarily in bound to this session.
		/// Only `Device::serial` and `Device::info` may be called on a Device that is not added to
		/// the session.
		std::vector<std::shared_ptr<Device>> m_available_devices;

		/// Add a device (from m_available_devices) to the session.
		/// This method may not be called while the session is active.
		Device* add_device(Device*);

		/// Devices that are part of this session. These devices will be started when start() is called.
		/// Use `add_device` and `remove_device` to manipulate this list.
		std::set<Device*> m_devices;

		/// get the device matching a given serial from the session
		Device* get_device(const char* serial);

		/// Remove a device from the session.
		/// This method may not be called while the session is active
		void remove_device(Device*);

		/// Remove a device from the list of available devices.
		/// Devices are automatically added to this list on attach.
		/// Devies must be removed from this list on detach.
		/// This method may not be called while the session is active
		void destroy_available(Device*);

		/// Configure the session's sample rate.
		/// This method may not be called while the session is active.
		void configure(uint64_t sampleRate);

		/// Run the currently configured capture and wait for it to complete
		void run(uint64_t nsamples);

		/// Start the currently configured capture, but do not wait for it to complete. Once started,
		/// the only allowed Session methods are cancel() and end() until the session has stopped.
		void start(uint64_t nsamples);

		/// Cancel capture and block waiting for it to complete
		void cancel();

		/// Update device firmware for a given device. When device is NULL the
		/// first attached device will be used instead.
		void flash_firmware(const char *file, Device* device = NULL);

		/// internal: Called by devices on the USB thread when they are complete
		void completion();

		/// internal: Called by devices on the USB thread when a device encounters an error
		void handle_error(int status, const char * tag);

		/// internal: Called by devices on the USB thread with progress updates
		void progress();
		/// internal: called by hotplug events on the USB thread
		void attached(libusb_device* device);
		void detached(libusb_device* device);

		/// Block until all devices have completed
		void wait_for_completion();

		/// Block until all devices have completed, then turn off the devices
		void end();

		/// Callback called on the USB thread with the sample number as samples are received
		std::function<void(uint64_t)> m_progress_callback;

		/// Callback called on the USB thread on completion
		std::function<void(unsigned)> m_completion_callback;

		/// Callback called on the USB thread when a device is plugged into the system
		std::function<void(Device* device)> m_hotplug_detach_callback;

		/// Callback called on the USB thread when a device is removed from the system
		std::function<void(Device* device)> m_hotplug_attach_callback;

		unsigned m_cancellation = 0;

	protected:
		uint64_t m_min_progress = 0;

		void start_usb_thread();
		std::thread m_usb_thread;
		bool m_usb_thread_loop;

		std::mutex m_lock;
		std::mutex m_lock_devlist;
		std::condition_variable m_completion;

		libusb_context* m_usb_cx;

		std::shared_ptr<Device> probe_device(libusb_device* device);
		std::shared_ptr<Device> find_existing_device(libusb_device* device);
	};

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

	class Signal {
	public:
		/// internal: Do not call the constructor directly; obtain a Signal from a Device
		Signal(const sl_signal_info* info): m_info(info), m_src(SRC_CONSTANT), m_src_v1(0), m_dest(DEST_NONE) {}

		/// Get the descriptor struct of the Signal.
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

		/// Configure received samples to be stored into `buf`, up to `len` points.
		/// After `len` points, samples will be dropped.
		void measure_buffer(float* buf, size_t len);

		/// Configure received samples to be passed to the provided callback.
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

		float* m_src_buf;
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

#endif // __LIBSMU_HPP__
