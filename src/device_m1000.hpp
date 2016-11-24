// Released under the terms of the BSD License
// (C) 2014-2016
//   Analog Devices, Inc.
//   Kevin Mehall <km@kevinmehall.net>
//   Ian Daniher <itdaniher@gmail.com>

#pragma once

#include <cstdint>
#include <array>
#include <mutex>
#include <vector>

#include <boost/lockfree/spsc_queue.hpp>
#include <libusb.h>

#include "debug.hpp"
#include "usb.hpp"
#include <libsmu/libsmu.hpp>

#define EEPROM_VALID 0x01ee02dd

static const sl_signal_info m1000_signal_info[2] = {
	{"Voltage", 0x7, 0x2, 0.0, 5.0, 5.0/65536},
	{"Current", 0x6, 0x4, -0.2, 0.2, 0.4/65536},
};

// Calibration data format stored in the device's EEPROM.
struct EEPROM_cal {
	uint32_t eeprom_valid;
	float offset[8];
	float gain_p[8];
	float gain_n[8];
};

namespace smu {
	extern "C" void LIBUSB_CALL m1000_in_completion(libusb_transfer *t);
	extern "C" void LIBUSB_CALL m1000_out_completion(libusb_transfer *t);

	class M1000_Device: public Device {
	public:
		// Handle incoming USB transfers.
		void in_completion(libusb_transfer *t);
		// Handle outgoing USB transfers.
		void out_completion(libusb_transfer *t);

		// Override virtual methods of the base Device class.
		const sl_device_info* info() const override;
		const sl_channel_info* channel_info(unsigned channel) const override;
		Signal* signal(unsigned channel, unsigned signal) override;
		int set_mode(unsigned channel, unsigned mode) override;
		int fwver_sem(std::array<unsigned, 3>& components) override;
		ssize_t read(std::vector<std::array<float, 4>>& buf, size_t samples, int timeout) override;
		int write(std::vector<float>& buf, unsigned channel) override;
		int sync() override;
		int write_calibration(const char* cal_file_name) override;
		int read_calibration() override;
		void calibration(std::vector<std::vector<float>>* cal) override;
		int samba_mode() override;

	protected:
		friend class Session;
		friend void LIBUSB_CALL m1000_in_completion(libusb_transfer *t);
		friend void LIBUSB_CALL m1000_out_completion(libusb_transfer *t);

		Signal m_signals[2][2];
		unsigned m_mode[2];

		// Ringbuffer with ~100ms worth of incoming sample values at the default rate.
		// The sample values are formatted in arrays of four values,
		// specifically in the following order: <ChanA voltage, ChanA current, ChanB voltage, ChanB current>.
		boost::lockfree::spsc_queue<std::array<float, 4>> m_in_samples_q;

		// Ringbuffers with ~100ms worth of outgoing sample values for both channels at the default rate.
		boost::lockfree::spsc_queue<float> m_out_samples_a_q;
		boost::lockfree::spsc_queue<float> m_out_samples_b_q;

		// Threads used to write outgoing samples values to the queues above.
		std::thread m_out_samples_a_thr;
		std::thread m_out_samples_b_thr;

		M1000_Device(Session* s, libusb_device* usb_dev):
			Device(s, usb_dev),
			m_signals {
				{Signal(&m1000_signal_info[0]), Signal(&m1000_signal_info[1])},
				{Signal(&m1000_signal_info[0]), Signal(&m1000_signal_info[1])},
			},
			m_mode{0,0},
			m_in_samples_q{s->m_queue_size},
			m_out_samples_a_q{s->m_queue_size},
			m_out_samples_b_q{s->m_queue_size}
			{}

		// Submit data transfers to usb thread, from host to device.
		int submit_out_transfer(libusb_transfer* t);

		// Submit data transfers to usb thread, from device to host.
		int submit_in_transfer(libusb_transfer* t);

		// Reformat received data, performs integer to float conversion.
		void handle_in_transfer(libusb_transfer* t);

		// Reformat outgoing data, performs float to integer conversion.
		void handle_out_transfer(libusb_transfer* t);

		// Encode output samples.
		// @param chan Target channel index.
		uint16_t encode_out(unsigned chan);

		unsigned m_packets_per_transfer;
		Transfers m_in_transfers;
		Transfers m_out_transfers;

		// Device calibration data.
		EEPROM_cal m_cal;

		// Number of requested samples.
		uint64_t m_sample_count = 0;

		// Override virtual methods of the base Device class.
		int get_default_rate() override;
		int claim() override;
		int release() override;
		int configure(uint64_t sampleRate) override;
		int on() override;
		int off() override;
		int cancel() override;
		int run(uint64_t samples) override;
	};
}
