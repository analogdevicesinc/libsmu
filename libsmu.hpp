// Released under the terms of the BSD License
// (C) 2014-2015
//   Kevin Mehall <km@kevinmehall.net>
//   Ian Daniher <itdaniher@gmail.com>

#pragma once
#include "libsmu.h"
#include <string>
#include <vector>
#include <set>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <cmath>

#ifndef M_PI
#define M_PI (4.0*atan(1.0))
#endif

class Device;
class Signal;
struct libusb_device;
struct libusb_device_handle;
struct libusb_context;


class Session {
public:
	Session();
	~Session();

	int update_available_devices();
	unsigned m_active_devices;
	std::vector<std::shared_ptr<Device>> m_available_devices;

	Device* add_device(Device*);
	std::set<Device*> m_devices;
	void remove_device(Device*);
	void configure(uint64_t sampleRate);
	std::function<void(sample_t)> m_progress_cb;

	// Run the currently configured capture and wait for it to complete
	void run(sample_t nsamples);
	void start(sample_t nsamples);
	void cancel();

	/// Called by devices on the USB thread when they are complete
	void completion();

	/// Called on the USB thread when a device encounters an error
	void handle_error(unsigned status);

	/// Called by devices on the USB thread with progress updates
	void progress();
	// called by hotplug events on the USB thread
	void attached(libusb_device* device);
	void detached(libusb_device* device);

	/// Block until all devices have completed, then turn off the devices
	void end();

	std::function<void(sample_t)> m_progress_callback;
	std::function<void(unsigned)> m_completion_callback;
	std::function<void(Device* device)> m_hotplug_detach_callback;
	std::function<void(Device* device)> m_hotplug_attach_callback;

	unsigned m_cancellation;

protected:
	sample_t m_min_progress;

	void start_usb_thread();
	std::thread m_usb_thread;
	bool m_usb_thread_loop;

	std::mutex m_lock;
	std::condition_variable m_completion;


	libusb_context* m_usb_cx;

	std::shared_ptr<Device> probe_device(libusb_device* device);
	std::shared_ptr<Device> find_existing_device(libusb_device* device);
};

class Device {
public:
	virtual ~Device();
	virtual const sl_device_info* info() const = 0;
	virtual const sl_channel_info*  channel_info(unsigned channel) const = 0;
	virtual Signal* signal(unsigned channel, unsigned signal) = 0;
	virtual const char* serial() const { return this->serial_num; }
	virtual void set_mode(unsigned channel, unsigned mode) = 0;
	void ctrl_transfer(unsigned bmRequestType, unsigned bRequest, unsigned wValue, unsigned wIndex, unsigned char *data, unsigned wLength, unsigned timeout);
	virtual int get_default_rate() {return 10000;};
	virtual void sync() {};

	virtual void lock() { m_state.lock(); }
	virtual void unlock() { m_state.unlock(); }


protected:
	Device(Session* s, libusb_device* d);
	virtual int init();
	virtual int added() {return 0;}
	virtual int removed() {return 0;}
	virtual void configure(uint64_t sampleRate) = 0;

	virtual void on() = 0;
	virtual void off() = 0;
	virtual void start_run(sample_t nsamples) = 0;
	virtual void cancel() = 0;

	Session* const m_session;
	libusb_device* const m_device = NULL;
	libusb_device_handle* m_usb = NULL;

	// State owned by USB thread
	sample_t m_requested_sampleno;
	sample_t m_in_sampleno;
	sample_t m_out_sampleno;

	std::mutex m_state;

	char serial_num[32];
	friend class Session;
};

enum Dest {
	DEST_NONE,
	DEST_BUFFER,
	DEST_CALLBACK,
};

enum Src {
	SRC_CONSTANT,
	SRC_SQUARE,
	SRC_SAWTOOTH,
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

class Signal {
public:
	Signal(const sl_signal_info* info): m_info(info), m_src(SRC_CONSTANT), m_src_v1(0), m_dest(DEST_NONE) {}
	const sl_signal_info* info() const { return m_info; }
	const sl_signal_info* const m_info;

	void source_constant(value_t val) {
		m_src = SRC_CONSTANT;
		m_src_v1 = val;
	}
	void source_square(value_t v1, value_t v2, double period, double duty, double phase) {
		m_src = SRC_SQUARE;
		update_phase(period, phase);
		m_src_v1 = v1;
		m_src_v2 = v2;
		m_src_duty = duty;
	}
	void source_sawtooth(value_t v1, value_t v2, double period, double phase) {
		m_src = SRC_SAWTOOTH;
		update_phase(period, phase);
		m_src_v1 = v1;
		m_src_v2 = v2;
	}
	void source_sine(value_t center, value_t amplitude, double period, double phase) {
		m_src = SRC_SINE;
		update_phase(period, phase);
		m_src_v1 = center;
		m_src_v2 = amplitude;
	}
	void source_triangle(value_t v1, value_t v2, double period, double phase) {
		m_src = SRC_TRIANGLE;
		update_phase(period, phase);
		m_src_v1 = v1;
		m_src_v2 = v2;
	}
	//void source_arb(arb_point_t* points, size_t len, bool repeat);
	void source_buffer(value_t* buf, size_t len, bool repeat) {
		m_src = SRC_BUFFER;
		m_src_buf = buf;
		m_src_buf_len = len;
		m_src_buf_repeat = repeat;
		m_src_i = 0;
	}
	void source_callback(std::function<value_t (sample_t index)> callback) {
		m_src = SRC_CALLBACK;
		m_src_callback = callback;
		m_src_i = 0;
	}

	value_t measure_instantaneous();
	void measure_buffer(value_t* buf, size_t len) {
		m_dest = DEST_BUFFER;
		m_dest_buf = buf;
		m_dest_buf_len = len;
	}

	void measure_callback(std::function<void(value_t value)> callback) {
		m_dest = DEST_CALLBACK;
		m_dest_callback = callback;
	}

	inline void put_sample(value_t val) {
		m_latest_measurement = val;
		if (m_dest == DEST_BUFFER) {
			if (m_dest_buf_len) {
				*m_dest_buf++ = val;
				m_dest_buf_len -= 1;
			}
		} else if (m_dest == DEST_CALLBACK) {
			m_dest_callback(val);
		}
	}

	inline value_t get_sample() {
		switch (m_src) {
		case SRC_CONSTANT:
			return m_src_v1;

		case SRC_BUFFER:
			if (m_src_i >= m_src_buf_len) {
				if (m_src_buf_repeat) {
					m_src_i = 0;
				} else {
					return m_src_buf[m_src_buf_len-1];
				}
			}
			return m_src_buf[m_src_i++];


		case SRC_CALLBACK:
			return m_src_callback(m_src_i++);

		case SRC_SQUARE:
		case SRC_SAWTOOTH:
		case SRC_SINE:
		case SRC_TRIANGLE:

			auto pkpk = m_src_v2 - m_src_v1;
			auto phase = m_src_phase;
			auto norm_phase = phase / m_src_period;
			m_src_phase = fmod(m_src_phase + 1, m_src_period);

			switch (m_src) {
			case SRC_SQUARE:
				return (norm_phase < m_src_duty) ? m_src_v1 : m_src_v2;

			case SRC_SAWTOOTH:
				return m_src_v1 + norm_phase * pkpk;

			case SRC_SINE:
				return m_src_v1 + (1 + cos(norm_phase * 2 * M_PI)) * pkpk/2;

			case SRC_TRIANGLE:
				return m_src_v1 + fabs(1 - norm_phase*2) * pkpk;
			default:
				return 0;
			}
		}
		return 0;
	}
	Src m_src;
	value_t m_src_v1;
	value_t m_src_v2;
	double m_src_period;
	double m_src_duty;
	double m_src_phase;

	value_t* m_src_buf;
	size_t m_src_i;
	size_t m_src_buf_len;
	bool m_src_buf_repeat;

	void update_phase(double new_period, double new_phase) {
		m_src_phase = new_phase;
		m_src_period = new_period;
	}

	std::function<value_t (sample_t index)> m_src_callback;

	Dest m_dest;

	// valid if m_dest == DEST_BUF
	value_t* m_dest_buf;
	size_t m_dest_buf_len;

	// valid if m_dest == DEST_CALLBACK
	std::function<void(value_t val)> m_dest_callback;

protected:

	value_t m_latest_measurement;
};
