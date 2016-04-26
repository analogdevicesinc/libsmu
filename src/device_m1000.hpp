// Released under the terms of the BSD License
// (C) 2014-2016
//   Analog Devices, Inc.
//   Kevin Mehall <km@kevinmehall.net>
//   Ian Daniher <itdaniher@gmail.com>

#pragma once

#include "internal.hpp"
#include <libsmu/device.hpp>
#include <libsmu/session.hpp>
#include <libsmu/signal.hpp>

#include <mutex>
#include <vector>

#include <libusb.h>

using std::vector;

extern "C" void LIBUSB_CALL m1000_in_completion(libusb_transfer *t);
extern "C" void LIBUSB_CALL m1000_out_completion(libusb_transfer *t);

#define EEPROM_VALID 0x01ee02dd

namespace smu {
	class M1000_Device: public Device {
	public:
		virtual ~M1000_Device();
		virtual const sl_device_info* info() const;
		virtual const sl_channel_info* channel_info(unsigned channel) const;
		//virtual sl_mode_info* mode_info(unsigned mode);
		virtual Signal* signal(unsigned channel, unsigned signal);
		virtual void set_mode(unsigned channel, unsigned mode);
		virtual void sync();
		virtual int write_calibration(const char* cal_file_name);
		virtual void calibration(vector<vector<float>>* cal);
		virtual void samba_mode();

		void in_completion(libusb_transfer *t);
		void out_completion(libusb_transfer *t);

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
		virtual void start_run(uint64_t nsamples);
		virtual void cancel();
		virtual void on();
		virtual void off();

		bool submit_out_transfer(libusb_transfer* t);
		bool submit_in_transfer(libusb_transfer* t);
		void handle_in_transfer(libusb_transfer* t);

		uint16_t encode_out(unsigned chan);

		unsigned m_packets_per_transfer;
		Transfers m_in_transfers;
		Transfers m_out_transfers;

		struct EEPROM_cal{
			uint32_t eeprom_valid;
			float offset[8];
			float gain_p[8];
			float gain_n[8];
		};

		void read_calibration();
		EEPROM_cal m_cal;

		uint64_t m_sample_count = 0;

		Signal m_signals[2][2];
		unsigned m_mode[2];
	};
}
