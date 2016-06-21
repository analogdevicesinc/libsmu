// Released under the terms of the BSD License
// (C) 2014-2016
//   Analog Devices, Inc.
//   Kevin Mehall <km@kevinmehall.net>
//   Ian Daniher <itdaniher@gmail.com>

/// @file session.hpp
/// @brief Session handling.

#pragma once

#include <cstdint>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

#include <libusb.h>

#include <libsmu/device.hpp>

namespace smu {
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
		/// @param device A pointer to the device to be added.
		/// @return On success, the pointer to the added device is returned.
		/// @return On error, NULL is returned.
		Device* add_device(Device* device);

		/// @brief Devices that are part of this session.
		/// These devices will be started when start() is called.
		/// Use `add_device` and `remove_device` to manipulate this list.
		std::set<Device*> m_devices;

		/// @brief Number of devices currently streaming samples.
		unsigned m_active_devices;

		/// @brief Get the device matching a given serial from the session.
		/// @param serial A pointer to the string for a device's serial number.
		/// @return On success, the pointer to the found device is returned.
		/// @return If no matching device is found, NULL is returned.
		Device* get_device(const char* serial);

		/// @brief Remove a device from the session.
		/// @param device A pointer to the device to be removed.
		/// This method may not be called while the session is active.
		void remove_device(Device* device);

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
}
