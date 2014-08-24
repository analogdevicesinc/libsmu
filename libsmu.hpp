#pragma once
#include "libsmu.h"
#include <string>
#include <vector>
#include <set>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <thread>

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
	std::vector<std::shared_ptr<Device>> m_available_devices;

	Device* add_device(Device*);
	std::set<Device*> m_devices;
	void remove_device(Device*);

	void configure(uint64_t sampleRate);
	std::function<void(sample_t)> m_progress_cb;

	// Run the currently configured capture and wait for it to complete
	void run(sample_t nsamples);
	void run_nonblocking(sample_t nsamples);
	void cancel();

	void completion();
protected:
	void start_usb_thread();
	std::thread m_usb_thread;
	bool m_usb_thread_loop;

	std::mutex m_lock;
	std::condition_variable m_completion;

	unsigned m_active_devices;

	libusb_context* m_usb_cx;
	std::shared_ptr<Device> probe_device(libusb_device* device);
	std::shared_ptr<Device> find_existing_device(libusb_device* device);
};

class Device {
public:
	virtual ~Device();
	virtual const sl_device_info* const info() = 0;
	virtual const sl_channel_info* const channel_info(unsigned channel) = 0;
	virtual Signal* signal(unsigned channel, unsigned signal) = 0;
	virtual const char* const serial() { return ""; }

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
	libusb_device* const m_device;
	libusb_device_handle* m_usb;

	friend class Session;
};

class Signal {
public:
	Signal(const sl_signal_info* info): m_info(info), m_dest(DEST_NONE) {}
	const sl_signal_info* const info() { return m_info; }
	const sl_signal_info* const m_info;

	void source_constant(value_t val);
	void source_square(value_t v1, value_t v2, sample_t t1, sample_t t2, int phase);
	void source_sawtooth(value_t v1, value_t v2, sample_t period, int phase);
	void source_sine(value_t center, value_t amplitude, double period, int phase);
	void source_triangle(value_t center, value_t amplitude, double period, int phase);
	//void source_arb(arb_point_t* points, size_t len, bool repeat);
	void source_buffer(value_t* buf, size_t len, bool repeat);
	void source_callback(std::function<void(sample_t index, size_t count, value_t* buf)>);

	value_t measure_instantaneous();
	void measure_buffer(value_t* buf, size_t len, bool repeat);
	void measure_callback(std::function<void(sample_t index, size_t count, value_t* buf)>);
protected:
	value_t latest_measurement;
};
