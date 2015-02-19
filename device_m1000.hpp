// Released under the terms of the BSD License
// (C) 2014-2015
//   Kevin Mehall <km@kevinmehall.net>
//   Ian Daniher <itdaniher@gmail.com>

#pragma once
#include <mutex>
#include "libsmu.hpp"
#include "internal.hpp"

struct libusb_device_handle;
extern "C" void LIBUSB_CALL m1000_in_completion(libusb_transfer *t);
extern "C" void LIBUSB_CALL m1000_out_completion(libusb_transfer *t);

class M1000_Device: public Device {
public:
	virtual ~M1000_Device();
	virtual const sl_device_info* info() const;
	virtual const sl_channel_info* channel_info(unsigned channel) const;
	virtual const sl_mode_info* mode_info(unsigned channel, unsigned mode) const;
	virtual Signal* signal(unsigned channel, unsigned signal);
	virtual void set_mode(unsigned channel, unsigned mode);
	virtual void sync();

protected:
	friend class Session;
	friend void LIBUSB_CALL m1000_in_completion(libusb_transfer *t);
	friend void LIBUSB_CALL m1000_out_completion(libusb_transfer *t);

	M1000_Device(Session* s, libusb_device* device);
	virtual int init();
	virtual int get_default_rate();
	virtual int added();
	virtual int removed();
	virtual void configure(uint64_t sampleRate);
	virtual void start_run(sample_t nsamples);
	virtual void cancel();
	virtual void on();
	virtual void off();

	void in_completion(libusb_transfer *t);
	void out_completion(libusb_transfer *t);

	bool submit_out_transfer(libusb_transfer* t);
	bool submit_in_transfer(libusb_transfer* t);
	void handle_in_transfer(libusb_transfer* t);

	uint16_t encode_out(int chan);

	std::string m_hw_version;
	std::string m_fw_version;
	std::string m_git_version;

	unsigned m_packets_per_transfer;
	Transfers m_in_transfers;
	Transfers m_out_transfers;

	uint64_t m_sample_rate;
	uint64_t m_sample_count;

	Signal m_signals[2][2];
	unsigned m_mode[2];
};
