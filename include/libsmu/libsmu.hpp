// Released under the terms of the BSD License
// (C) 2014-2016
//   Analog Devices, Inc.
//   Kevin Mehall <km@kevinmehall.net>
//   Ian Daniher <itdaniher@gmail.com>

/// @file libsmu.hpp
/// @brief Main public interface.

#pragma once

#include <cstdint>
#include <array>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include <libusb.h>

#include <libsmu/version.hpp>

/// @brief List of supported devices.
/// The list uses the vendor and project IDs from USB
/// information formatted as {vendor_id, product_id}.
const std::vector<std::vector<uint16_t>> SUPPORTED_DEVICES = {
	{0x0456, 0xcee2}, // old
	{0x064b, 0x784c}, // new
};

/// @brief List of supported devices in SAM-BA bootloader mode.
/// The list uses the vendor and project IDs from USB information
/// formatted as {vendor_id, product_id}.
const std::vector<std::vector<uint16_t>> SAMBA_DEVICES = {
	{0x03eb, 0x6124}, // shows up as a CDC device by default
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
	CONSTANT, ///< Constant value output.
	SQUARE, ///< Square wave output.
	SAWTOOTH, ///< Sawtooth wave output.
	STAIRSTEP, ///< Stairstep wave output.
	SINE, ///< Sine wave output.
	TRIANGLE, ///< Triangle wave output.
};

/// @brief Supported channel modes.
enum Mode {
	HI_Z, ///< Channel is floating.
	SVMI, ///< Source voltage, measure current.
	SIMV, ///< Source current, measure voltage.
	HI_Z_SPLIT,
	SVMI_SPLIT,
	SIMV_SPLIT,
};

enum LED{
    RED = 47,
    GREEN = 29,
    BLUE = 28,
    ALL = 0
};

namespace smu {
	class Device;
	class Signal;

	/// @brief Generic session class.
	class Session {
	public:
		Session();
		~Session();

		/// @brief Devices that are present on the system.
		/// Note that these devices consist of all supported devices currently
		/// recognized on the system; however, the devices aren't necessarily
		/// bound to a session. In order to add devices to a session, add()
		/// must be used.
		std::vector<Device*> m_available_devices;

		/// @brief Devices that are part of this session.
		/// These devices will be started when start() is called.
		/// Use `add()` and `remove()` to manipulate this set.
		std::set<Device*> m_devices;

		/// @brief Number of devices currently streaming samples.
		unsigned m_active_devices;

		/// @brief Size of input/output sample queues for every device.
		/// Alter this if necessary to make continuous data flow work for the
		/// target usage. The default is approximately 100ms worth of samples
		/// at the maximum sampling rate. Note that must be changed before
		/// devices are added to a session otherwise they'll use the default.
		unsigned m_queue_size = 100000;


        unsigned m_samples;
		/// @brief Scan system for all supported devices.
		/// Updates the list of available, supported devices for the session
		/// (m_available_devices).
		/// @return On success, the number of devices found is returned.
		/// @return On error, a negative errno code is returned.
		int scan();

		/// @brief Add a device to the session.
		/// This method may not be called while the session is active.
		/// @param device The device to be added to the session.
		/// @return On success, 0 is returned.
		/// @return On error, a negative errno code is returned.
		int add(Device* device);

		/// @brief Shim to scan and add all available devices to a session.
		/// This method may not be called while the session is active.
		/// @return On success, the number of devices added to the session is returned.
		/// @return On error, a negative errno code is returned.
		int add_all();

		/// @brief Remove a device from the session.
		/// @param device A device to be removed from the session.
		/// @param detached True if the device has already been detached from
		/// the system (defaults to false).
		/// This method may not be called while the session is active.
		/// @return On success, 0 is returned.
		/// @return On error, a negative errno code is returned.
		int remove(Device* device, bool detached = false);

		/// @brief Remove a device from the list of available devices.
		/// @param device A device to be removed from the available list.
		/// Devices are automatically added to this list on attach.
		/// Devices must be removed from this list on detach.
		/// This method may not be called while the session is active.
		/// @return On success, 0 is returned.
		/// @return On error, a negative errno code is returned.
		int destroy(Device* device);

		/// @brief Configure the session's sample rate.
		/// @param sampleRate The requested sample rate for the session.
		/// Requesting a sample rate of 0 (the default) causes the session to
		/// use the first device's default sample rate.
		/// @return On success, the configured sample rate is returned.
		/// @return On error, a negative errno code is returned.
		/// This method may not be called while the session is active.
		int configure(uint32_t sampleRate = 0);

		/// @brief Run the currently configured capture and wait for it to complete.
		/// @param samples Number of samples to capture until we stop. If 0, run in continuous mode.
		/// @return On success, 0 is returned.
		/// @return On error, a negative errno code is returned.
		/// @throws std::system_error of EBUSY if sample underflows/overflows have occurred.
		int run(uint64_t samples);

		/// @brief Start the currently configured capture, but do not wait for it to complete.
		/// @param samples Number of samples to capture until we stop. If 0, run in continuous mode.
		///
		/// Note that the number of samples actually captured will be the
		/// nearest multiple of the amount of samples per USB packet larger than
		/// the request amount of samples.
		///
		/// Once started, the only allowed Session methods are cancel() and end()
		/// until the session has stopped.
		///
		/// @return On success, 0 is returned.
		/// @return On error, a negative errno code is returned.
		/// @throws std::system_error of EBUSY if sample underflows/overflows have occurred.
		int start(uint64_t samples);

		/// @brief Cancel capture and block waiting for it to complete.
		/// @return On success, 0 is returned.
		/// @return On error, -1 is returned. Note that the cancellation
		/// process will stop on the first device that fails to cancel its
		/// capture.
		int cancel();

		/// @brief Determine the cancellation status of a session.
		/// @return True, if the session has been cancelled (usually from
		/// explicitly calling cancel() or cancelled USB transactions).
		/// @return False, if the session hasn't been started, is running, or
		/// has been stopped successfully.
		bool cancelled() { return m_cancellation != 0; }

		/// @brief Flush the read and write queues for all devices in a session.
		void flush();

		/// @brief Scan system for devices in SAM-BA mode.
		/// @param samba_devs Vector of libusb devices in SAM-BA mode. 
		/// @return On success, the number of devices found is returned.
		/// @return On error, a negative errno code is returned.
		int scan_samba_devs(std::vector<libusb_device*>& samba_devs);

		/// @brief Update firmware for the target device(s).
		/// @param file Path to firmware file.
		/// @param devices The device(s) targeted for updating. If empty or
		/// unset, all the attached devices in SAM-BA mode are updated.
		/// @return On success, the number of devices flashed is returned.
		/// @throws std::runtime_error for various USB failures causing aborted flashes.
		int flash_firmware(std::string file, std::vector<Device*> devices = {});

		/// internal: Called by devices on the USB thread when they are complete.
		void completion();
		/// internal: Called by devices on the USB thread when a device encounters an error.
		void handle_error(int status, const char * tag);
		/// internal: Called by device attach events on the USB thread.
		void attached(libusb_device* usb_dev);
		/// internal: Called by device detach events on the USB thread.
		void detached(libusb_device* usb_dev);

		/// @brief For noncontinuous sessions, block until all devices have completed,
		/// then turn off the devices. Continuous sessions don't wait for completion
		/// and instead proceed with turning off the devices after canceling the session.
		/// @return On success, 0 is returned.
		/// @return On error, a negative errno code is returned.
		int end();

		/// @brief Callback run via the USB thread on session completion.
		/// Called with the current value of m_cancellation as an argument,
		/// i.e. if the parameter is non-zero we are waiting to complete a
		/// cancelled session.
		std::function<void(unsigned)> m_completion_callback;

		/// @brief Session sample rate.
		uint64_t m_sample_rate = 0;

		/// @brief Flag used to determine if a session is in continuous mode or not.
		bool m_continuous = false;

	protected:
		/// @brief Flag used to cancel all pending USB transactions for devices in a session.
		unsigned m_cancellation = 0;

		/// @brief Flag for controlling USB event handling.
		/// USB event handling loop will be run while m_usb_thread_loop is true.
		std::atomic<bool> m_usb_thread_loop;
		/// @brief USB thread handling pending events in blocking mode.
		std::thread m_usb_thread;

		/// @brief Lock for session completion.
		std::mutex m_lock;
		/// @brief Lock for the available device list.
		/// All code that references m_available_devices needs to acquire this lock
		/// before accessing it.
		std::mutex m_lock_devlist;
		/// @brief Blocks on m_lock until session completion is finished.
		std::condition_variable m_completion;

		/// @brief libusb context related with a session.
		/// This allows for segregating libusb usage so external users can
		/// also use libusb without interfering with internal usage.
		libusb_context* m_usb_ctx;

		/// @brief libusb hotplug callback handle.
		libusb_hotplug_callback_handle m_usb_cb;

		/// @brief Callbacks called on the USB thread when a device is removed from the system.
		std::vector<std::function<void(Device* device)>> m_hotplug_attach_callbacks;
		/// @brief Callbacks called on the USB thread when a device is plugged into the system.
		std::vector<std::function<void(Device* device)>> m_hotplug_detach_callbacks;

		/// @brief Identify devices supported by libsmu.
		/// @param usb_dev libusb device
		/// @return If the usb device relates to a supported device the Device is returned,
		/// otherwise NULL is returned.
		Device* probe_device(libusb_device* usb_dev);

		/// @brief Find an existing, available device.
		/// @param usb_dev libusb device
		/// @return If the usb device relates to an existing,
		/// available device the Device is returned,
		/// otherwise NULL is returned.
		Device* find_existing_device(libusb_device* usb_dev);
	};

	/// @brief Generic device class.
	class Device {
	public:
		virtual ~Device() {};

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

		/// hardware version
		const std::string m_hwver;
		/// firmware version
		const std::string m_fwver;
		/// serial number
		const std::string m_serial;

		/// @brief Get the array of firmware version components (major, minor, patch).
		/// Note that this method assumes semantic versioning so versions such
		/// as 2.06 will be coerced to 2.6.0, i.e. major=2, minor=6, patch=0.
		virtual int fwver_sem(std::array<unsigned, 3>& components) = 0;

		/// @brief Set a custom serial number for the device.
		/// @param serial The custom serial number to set, defaults to an empty
		/// string that causes the device to reset back to its original serial number.
		/// @return On success, 0 is returned.
		/// @return On error, a negative integer is returned relating to the error status.
		/// This method may not be called while the session is active.
		virtual int set_serial(std::string serial = "") = 0;

		/// @brief Set the mode of the specified channel.
		/// @param channel An unsigned integer relating to the requested channel.
		/// @param mode An unsigned integer relating to the requested mode.
		/// @param restore Boolean relating to supporting saving the mode being
		//  set in order to restore it for the next data acquisition, defaults to true.
		/// @return On success, 0 is returned.
		/// @return On error, a negative integer is returned relating to the error status.
		/// This method may not be called while the session is active.
		virtual int set_mode(unsigned channel, unsigned mode, bool restore = true) = 0;

		/// @brief Get the mode of the specified channel.
		/// @param channel An unsigned integer relating to the requested channel.
		/// @return The mode of the specified channel.
		/// @return On error, a negative integer is returned relating to the error status.
		virtual int get_mode(unsigned channel) = 0;

		/// @brief Get all signal samples from a device.
		/// @param buf Buffer object to store sample values into.
		/// @param samples Number of samples to read.
		/// @param timeout Amount of time in milliseconds to wait for samples.
		/// @param skipsamples Whether to skip the queued samples.
		/// to be available. If 0 (the default), return immediately. If -1,
		/// block indefinitely until the requested number of samples is available.
		/// @return On success, the number of samples read.
		/// @return On error, a negative integer is returned relating to the error status.
		/// @throws std::system_error of EBUSY if sample overflows have occurred.
		virtual ssize_t read(std::vector<std::array<float, 4>>& buf, size_t samples, int timeout = 0, bool skipsamples = false) = 0;

		/// @brief Write data to a specified channel of the device.
		/// @param buf Buffer of samples to write to the specified channel.
		/// @param channel Channel to write samples to.
		/// @param cyclic Enable cyclic mode (passed buffer is looped over continuously).
		/// @return On success, 0 is returned.
		/// @return On error, a negative integer is returned relating to the error status.
		/// @throws std::system_error of EBUSY if sample underflows have occurred.
		virtual int write(std::vector<float>& buf, unsigned channel, bool cyclic = false) = 0;

		/// @brief Flush the read and selected channel write queue for a device.
		/// @param channel Channel to flush the write queues for. If -1, skip flushing write queues.
		/// @param read Whether to flush the incoming read queue as well.
		virtual void flush(int channel, bool read = false) = 0;

		/// @brief Perform a raw USB control transfer on the underlying USB device.
		/// @return Passes through the return value of the underlying libusb_control_transfer method.
		/// See the libusb_control_transfer() docs for parameter descriptions.
		int ctrl_transfer(unsigned bmRequestType, unsigned bRequest, unsigned wValue, unsigned wIndex,
						unsigned char *data, unsigned wLength, unsigned timeout);

		/// @brief Force the device into SAM-BA bootloader mode.
		/// @return On success, 0 is returned.
		/// @return On error, a negative errno code is returned.
		virtual int samba_mode() = 0;

		/// @brief Get the default sample rate.
		virtual int get_default_rate() { return 100000; }

		/// @brief Prepare multi-device synchronization.
		/// Get current microframe index, set m_sof_start to be time in the future.
		/// @return On success, 0 is returned.
		/// @return On error, a negative errno code is returned.
		virtual int sync() = 0;

		/// @brief Lock the device's mutex.
		/// This prevents this device's transfers from being processed. Hold
		/// only briefly, while modifying signal state.
		virtual void lock() { m_state.lock(); }

		/// @brief Unlock the device's mutex.
		/// Allows this device's transfers to be processed.
		virtual void unlock() { m_state.unlock(); }

		/// @brief Write the device calibration data into the EEPROM.
		/// @param cal_file_name The path to a properly-formatted calibration
		/// data file to write to the device. If NULL is passed, calibration
		/// is reset to the default setting.
		/// @return On success, 0 is returned.
		/// @return On error, a negative integer is returned relating to the error status.
		virtual int write_calibration(const char* cal_file_name) { return 0; }

		/// @brief Read device calibration data from the EEPROM.
		virtual int read_calibration() = 0;

		/// @brief Get the device calibration data from the EEPROM.
		/// @param cal A vector of vectors containing calibration values.
		virtual void calibration(std::vector<std::vector<float>>* cal) = 0;

		/// @brief Session this device is associated with.
		/// @brief Overcurrent status for the most recent data request.
		///   Is 1 if an overcurrent event occurred in the most recent data request, 0 otherwise.
		int m_overcurrent = 0;
		
		/// @brief Set the leds states for device.
        /// @param leds value between [0, 7], each bit of the value represents the state of an LED (1-on 0-off) in this order (RGB or DS3,DS2,DS1 on rev F hardware)
        virtual int set_led(unsigned leds) = 0;
		/// set adc mux mode
		virtual int set_adc_mux(unsigned adc_mux) = 0;

	protected:
		/// @brief Device constructor.
		Device(Session* s, libusb_device* usb_dev, libusb_device_handle* usb_handle,
			const char* hw_version, const char* fw_version, const char* serial);

		/// @brief Device claiming and initialization when a session adds this device.
		/// @return On success, 0 is returned.
		/// @return On error, a negative errno code is returned.
		virtual int claim() { return 0; }

		/// @brief Device releasing when a session removes this device.
		/// @return On success, 0 is returned.
		/// @return On error, a negative errno code is returned.
		virtual int release() { return 0; }

		/// @brief Configurization and initialization for device sampling.
		/// @param sampleRate The requested sampling rate for the device.
		/// @return On success, the configured sample rate is returned.
		/// @return On error, a negative errno code is returned.
		virtual int configure(uint32_t sampleRate) = 0;

		/// @brief Turn on power supplies and clear sampling state.
		/// @return On success, 0 is returned.
		/// @return On error, a negative errno code is returned.
		virtual int on() = 0;

		/// @brief Stop any current sample writes and stop capturing samples.
		/// @return On success, 0 is returned.
		/// @return On error, a negative errno code is returned.
		virtual int off() = 0;

		/// @brief Make the device start sampling.
		/// @param samples Number of samples to run before stopping.
		/// @return On success, 0 is returned.
		/// @return On error, a negative errno code is returned.
		/// @throws std::system_error of EBUSY if sample underflows/overflows have occurred.
		virtual int run(uint64_t samples) = 0;

		/// @brief Cancel all pending libusb transactions.
		/// @return On success, 0 is returned.
		/// @return On error, a negative errno code is returned.
		virtual int cancel() = 0;

		/// @brief Session this device is associated with.
		Session* const m_session;

		/// @brief Underlying libusb device.
		libusb_device* const m_usb_dev = NULL;
		/// @brief Underlying libusb device handle.
		libusb_device_handle* m_usb = NULL;

		/// Cumulative sample number being handled for input.
		uint64_t m_requested_sampleno = 0;
		/// Current sample number being handled for input.
		uint64_t m_in_sampleno = 0;
		/// Current sample number being submitted for output.
		uint64_t m_out_sampleno = 0;

		/// @brief Amount of time in milliseconds to wait before timing out write
		/// operations, defaults to 100 ms and is based on the configured sample rate.
		double m_write_timeout = 100;

		/// Lock for transfer state.
		std::mutex m_state;

		friend class Session;
	};

	/// @brief Generic signal class.
	class Signal {
	public:
		/// internal: Do not call the constructor directly; obtain a Signal from a Device.
		Signal(const sl_signal_info* info = NULL):
			m_info(info)
			{}

		/// @brief Get the descriptor struct of the Signal.
		/// Pointed-to memory is valid for the lifetime of the Device.
		const sl_signal_info* info() const { return m_info; }
		/// Signal information.
		const sl_signal_info* const m_info;

		/// @brief Generate a constant waveform.
		/// @param buf Buffer object to store waveform into.
		/// @param samples Number of samples to create for the waveform.
		void constant(std::vector<float>& buf, uint64_t samples, float val);

		/// @brief Generate a square waveform.
		/// @param buf Buffer object to store waveform into.
		/// @param samples Number of samples to create for the waveform.
		/// @param midpoint Value at the middle of the wave.
		/// @param peak Maximum value of the wave.
		/// @param period Number of samples the wave takes for one cycle.
		/// @param phase Position in time (sample number) that the wave starts at.
		/// @param duty Duty cycle of the waveform (fraction of time in which the signal is active, e.g. 0.5 is half the time).
		void square(std::vector<float>& buf, uint64_t samples, float midpoint, float peak, double period, double phase, double duty);

		/// @brief Generate a sawtooth waveform.
		/// @param buf Buffer object to store waveform into.
		/// @param samples Number of samples to create for the waveform.
		/// @param midpoint Value at the middle of the wave.
		/// @param peak Maximum value of the wave.
		/// @param period Number of samples the wave takes for one cycle.
		/// @param phase Position in time (sample number) that the wave starts at.
		void sawtooth(std::vector<float>& buf, uint64_t samples, float midpoint, float peak, double period, double phase);

		/// @brief Generate a stairstep waveform.
		/// @param buf Buffer object to store waveform into.
		/// @param samples Number of samples to create for the waveform.
		/// @param midpoint Value at the middle of the wave.
		/// @param peak Maximum value of the wave.
		/// @param period Number of samples the wave takes for one cycle.
		/// @param phase Position in time (sample number) that the wave starts at.
		void stairstep(std::vector<float>& buf, uint64_t samples, float midpoint, float peak, double period, double phase);

		/// @brief Generate a sinusoidal waveform.
		/// @param buf Buffer object to store waveform into.
		/// @param samples Number of samples to create for the waveform.
		/// @param midpoint Value at the middle of the wave.
		/// @param peak Maximum value of the wave.
		/// @param period Number of samples the wave takes for one cycle.
		/// @param phase Position in time (sample number) that the wave starts at.
		void sine(std::vector<float>& buf, uint64_t samples, float midpoint, float peak, double period, double phase);

		/// @brief Generate a triangle waveform.
		/// @param buf Buffer object to store waveform into.
		/// @param samples Number of samples to create for the waveform.
		/// @param midpoint Value at the middle of the wave.
		/// @param peak Maximum value of the wave.
		/// @param period Number of samples the wave takes for one cycle.
		/// @param phase Position in time (sample number) that the wave starts at.
		void triangle(std::vector<float>& buf, uint64_t samples, float midpoint, float peak, double period, double phase);

	protected:
		Src m_src;
		float m_src_v1;
		float m_src_v2;
		double m_src_period;
		double m_src_duty;
		double m_src_phase;

		/// @brief Generate value for currently selected waveform.
		float get_sample();
	};
}
