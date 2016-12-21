// Released under the terms of the BSD License
// (C) 2014-2016
//   Analog Devices, Inc.
//   Kevin Mehall <km@kevinmehall.net>
//   Ian Daniher <itdaniher@gmail.com>

#pragma once

#include <cstdint>
#include <cmath>
#include <array>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
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
		int get_mode(unsigned channel) override;
		int fwver_sem(std::array<unsigned, 3>& components) override;
		ssize_t read(std::vector<std::array<float, 4>>& buf, size_t samples, int timeout) override;
		int write(std::vector<float>& buf, unsigned channel, bool cyclic) override;
		void flush() override;
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

		~M1000_Device();

		// Queue with ~100ms worth of incoming sample values at the default rate.
		// The sample values are formatted in arrays of four values,
		// specifically in the following order: <ChanA voltage, ChanA current, ChanB voltage, ChanB current>.
		boost::lockfree::spsc_queue<std::array<float, 4>> m_in_samples_q;

		// Number of samples available for reading/writing.
		// TODO: Drop this when stable distros contain >= boost-1.57 with
		// read_available() and write_available() calls for the spsc queue.
		std::atomic<uint32_t> m_in_samples_avail;
		std::atomic<uint32_t> m_out_samples_avail[2] = {};

		// Queues with ~100ms worth of outgoing sample values for both channels at the default rate.
		boost::lockfree::spsc_queue<float> _out_samples_a_q;
		boost::lockfree::spsc_queue<float> _out_samples_b_q;

		// Reference the queues above via an array.
		boost::lockfree::spsc_queue<float>* m_out_samples_q[2] = {&_out_samples_a_q, &_out_samples_b_q};

		// Write buffers, one for each channel.
		std::vector<float> m_out_samples_buf[2];
		bool m_out_samples_buf_cyclic[2]{false,false};
		std::mutex m_out_samples_mtx[2];
		std::mutex m_out_samples_state_mtx[2];
		std::condition_variable m_out_samples_cv[2];

		// Used for write thread signaling, initialized to zero. If greater
		// than zero the related write thread will stop using its current
		// buffer and wait for another to be submitted. If less than zero, the
		// write thread will return -- used to signal the thread to exit.
		std::atomic<int> m_out_samples_stop[2] = {};

		// Threads used to write outgoing samples values to the queues above.
		std::thread m_out_samples_thr[2];

		// Used to keep initial USB transfer kickoff thread alive on Windows
		// until off() is called.
		std::condition_variable m_usb_cv;

		M1000_Device(Session* s, libusb_device* d, libusb_device_handle* h,
				const char* hw_version, const char* fw_version, const char* serial):
			Device(s, d, h, hw_version, fw_version, serial),
			m_signals {
				{Signal(&m1000_signal_info[0]), Signal(&m1000_signal_info[1])},
				{Signal(&m1000_signal_info[0]), Signal(&m1000_signal_info[1])},
			},
			m_mode{HI_Z,HI_Z},
			m_in_samples_q{s->m_queue_size},
			m_in_samples_avail{0},
			_out_samples_a_q{s->m_queue_size},
			_out_samples_b_q{s->m_queue_size}
			{}

		// Reformat received data, performs integer to float conversion.
		void handle_in_transfer(libusb_transfer* t);

		// Reformat outgoing data, performs float to integer conversion.
		void handle_out_transfer(libusb_transfer* t);

		// Submit data transfers to usb thread, from host to device.
		int submit_out_transfer(libusb_transfer* t);

		// Submit data transfers to usb thread, from device to host.
		int submit_in_transfer(libusb_transfer* t);

		// Encode output samples.
		// @param chan Target channel index.
		uint16_t encode_out(unsigned chan);

		// Most recent value written to the output of each channel initialized
		// to an invalid value in order to know when data hasn't been written
		// to a channel.
		float m_previous_output[2] = {std::nanf(""), std::nanf("")};

		// USB start of frame packet number.
		uint16_t m_sof_start = 0;

		// clock cycles per sample
		int m_sam_per = 0;
		// minimum clock cycles per sample (100ksps)
		const int m_min_per = 240;
		// maximum clock cycles per sample (~1024 samples/s)
		const int m_max_per = 24000;

		unsigned m_samples_per_transfer;
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
		int configure(unsigned sampleRate) override;
		int on() override;
		int off() override;
		int cancel() override;
		int run(uint64_t samples) override;
	};
}
