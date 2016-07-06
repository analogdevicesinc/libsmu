// Released under the terms of the BSD License
// (C) 2014-2016
//   Analog Devices, Inc.
//   Kevin Mehall <km@kevinmehall.net>
//   Ian Daniher <itdaniher@gmail.com>

#pragma once

#include <mutex>
#include <vector>

#include <libusb.h>

#include "debug.hpp"
#include "transfers.hpp"
#include <libsmu/libsmu.hpp>

extern "C" void LIBUSB_CALL m1000_in_completion(libusb_transfer *t);
extern "C" void LIBUSB_CALL m1000_out_completion(libusb_transfer *t);

#define EEPROM_VALID 0x01ee02dd

static const sl_signal_info m1000_signal_info[2] = {
	{ SIGNAL, "Voltage", 0x7, 0x2, unit_V, 0.0, 5.0, 5.0/65536 },
	{ SIGNAL, "Current", 0x6, 0x4, unit_A, -0.2, 0.2, 0.4/65536 },
};

struct EEPROM_cal {
	uint32_t eeprom_valid;
	float offset[8];
	float gain_p[8];
	float gain_n[8];
};

namespace smu {
	class M1000_Device: public Device {
	public:
		void in_completion(libusb_transfer *t);
		void out_completion(libusb_transfer *t);

		// Override virtual methods of the base Device class.
		const sl_device_info* info() const override;
		const sl_channel_info* channel_info(unsigned channel) const override;
		Signal* signal(unsigned channel, unsigned signal) override;
		void set_mode(unsigned channel, unsigned mode) override;
		void sync() override;
		int write_calibration(const char* cal_file_name) override;
		void calibration(std::vector<std::vector<float>>* cal) override;
		void samba_mode() override;

	protected:
		friend class Session;
		friend void LIBUSB_CALL m1000_in_completion(libusb_transfer *t);
		friend void LIBUSB_CALL m1000_out_completion(libusb_transfer *t);

		Signal m_signals[2][2];
		unsigned m_mode[2];

		M1000_Device(Session* s, libusb_device* device):
			Device(s, device),
			m_signals {
				{Signal(&m1000_signal_info[0]), Signal(&m1000_signal_info[1])},
				{Signal(&m1000_signal_info[0]), Signal(&m1000_signal_info[1])},
			},
			m_mode{0,0}
			{}

		virtual int init();
		virtual int get_default_rate();
		virtual int added();
		virtual int removed();

		/// calculate values for sampling period for SAM3U timer
		virtual void configure(uint64_t sampleRate);

		/// command device to start sampling
		virtual void start_run(uint64_t nsamples);

		/// cancel pending libusb transactions
		virtual void cancel();

		/// turn on power supplies, clear sampling state
		virtual void on();

		/// put outputs into high-impedance mode, stop sampling
		virtual void off();

		/// submit data transfers to usb thread - from host to device
		bool submit_out_transfer(libusb_transfer* t);

		/// submit data transfers to usb thread - from device to host
		bool submit_in_transfer(libusb_transfer* t);

		/// reformat received data - integer to float conversion
		void handle_in_transfer(libusb_transfer* t);

		/// encode output samples
		uint16_t encode_out(unsigned chan);

		unsigned m_packets_per_transfer;
		Transfers m_in_transfers;
		Transfers m_out_transfers;

		void read_calibration();
		EEPROM_cal m_cal;

		uint64_t m_sample_count = 0;
	};
}
