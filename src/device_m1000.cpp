// Released under the terms of the BSD License
// (C) 2014-2016
//   Analog Devices, Inc.
//   Kevin Mehall <km@kevinmehall.net>
//   Ian Daniher <itdaniher@gmail.com>

#include "device_m1000.hpp"

#include <cassert>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <array>
#include <chrono>
#include <exception>
#include <functional>
#include <iterator>
#include <system_error>
#include <stdexcept>
#include <thread>
#include <vector>

#include <boost/algorithm/string.hpp> // boost::split
#include <boost/lockfree/spsc_queue.hpp>
#include <libusb.h>

#include "debug.hpp"
#include "usb.hpp"
#include <libsmu/libsmu.hpp>

#define EP_OUT 0x02
#define EP_IN 0x81

const unsigned chunk_size = 256;
#define OUT_SAMPLES_PER_PACKET chunk_size
#define IN_SAMPLES_PER_PACKET chunk_size
#define CHAN_A 0
#define CHAN_B 1

const unsigned out_packet_size = chunk_size * 2 * 2;
const unsigned in_packet_size = chunk_size * 4 * 2;

const int m_min_per = 0x18;
volatile uint16_t m_sof_start = 0;
int m_sam_per = 0;

#ifdef _WIN32
const double BUFFER_TIME = 0.050;
#else
const double BUFFER_TIME = 0.020;
#endif

// Exception pointer to help move exceptions between USB and main threads.
std::exception_ptr e_ptr = nullptr;

using namespace smu;

static const sl_device_info m1000_info = {"ADALM1000", 2};

static const sl_channel_info m1000_channel_info[2] = {
	{"A", 3, 2},
	{"B", 3, 2},
};

int M1000_Device::get_default_rate()
{
	// rev0 firmware
	if (strcmp(m_fw_version, "023314a*") == 0) {
		return 62500;
	}
	// modern fw
	else {
		return 100000;
	}
}

int M1000_Device::claim()
{
	int ret = 0;
	ret = libusb_claim_interface(m_usb, 0);
	return -libusb_to_errno(ret);
}

int M1000_Device::release()
{
	int ret = 0;
	ret = libusb_release_interface(m_usb, 0);
	return -libusb_to_errno(ret);
}

int M1000_Device::read_calibration()
{
	int ret;

	ret = ctrl_transfer(0xC0, 0x01, 0, 0, (unsigned char*)&m_cal, sizeof(EEPROM_cal), 100);
	if (ret <= 0 || m_cal.eeprom_valid != EEPROM_VALID) {
		for (int i = 0; i < 8; i++) {
			m_cal.offset[i] = 0.0f;
			m_cal.gain_p[i] = 1.0f;
			m_cal.gain_n[i] = 1.0f;
		}
	}

	if (ret > 0)
		ret = 0;
	return ret;
}

void M1000_Device::calibration(std::vector<std::vector<float>>* cal)
{
	(*cal).resize(8);
	for (int i = 0; i < 8; i++) {
		(*cal)[i].resize(3);
		(*cal)[i][0] = m_cal.offset[i];
		(*cal)[i][1] = m_cal.gain_p[i];
		(*cal)[i][2] = m_cal.gain_n[i];
	}
}

int M1000_Device::write_calibration(const char* cal_file_name)
{
	int cal_records_no = 0;
	int ret;
	FILE* fp;
	char str[128];
	float ref[16], val[16];
	int rec_idx;
	int cnt_n, cnt_p;
	float gain_p, gain_n;

	// reset calibration to the defaults if NULL is passed
	if (cal_file_name == NULL) {
		for (int i = 0; i < 8; i++) {
			m_cal.offset[i] = 0.0f;
			m_cal.gain_p[i] = 1.0f;
			m_cal.gain_n[i] = 1.0f;
		}
		cal_records_no = 8;
		goto write_cal;
	}

	fp = fopen(cal_file_name, "r");
	if (!fp) {
		return -1;
	}

	while (fgets(str, 128, fp) != NULL) {
		if (strstr(str, "</>")) {
			rec_idx = 0;
			while (fgets(str, 128, fp) != NULL) {
				if (strstr(str, "<\\>") && rec_idx) {
					gain_p = 0;
					gain_n = 0;
					cnt_n = 0;
					cnt_p = 0;
					m_cal.offset[cal_records_no] = val[0] - ref[0];
					for (int i = 1; i < rec_idx; i++) {
						if (ref[i] > 0) {
							gain_p += ref[i] / (val[i] - m_cal.offset[cal_records_no]);
							cnt_p++;
						}
						else {
							gain_n += ref[i] / (val[i] - m_cal.offset[cal_records_no]);
							cnt_n++;
						}

					}
					m_cal.gain_p[cal_records_no] = cnt_p ? gain_p / (cnt_p) : 1.0f;
					m_cal.gain_n[cal_records_no] = cnt_n ? gain_n / (cnt_n) : 1.0f;
					cal_records_no++;
					break;
				}
				else {
					sscanf(str, "<%f, %f>", &ref[rec_idx], &val[rec_idx]);
					rec_idx++;
				}
			}
		}
	}

	fclose(fp);

write_cal:
	if (cal_records_no != 8) {
		// invalid calibration file
		ret = -EINVAL;
	} else {
		m_cal.eeprom_valid = EEPROM_VALID;
		ret = ctrl_transfer(0x40, 0x02, 0, 0, (unsigned char*)&m_cal, sizeof(EEPROM_cal), 100);
		if (ret > 0)
			ret = 0;
		else
			ret = -libusb_to_errno(ret);
	}

	return ret;
}

// Runs in USB thread
extern "C" void LIBUSB_CALL m1000_in_completion(libusb_transfer *t)
{
	if (!t->user_data) {
		libusb_free_transfer(t);
		return;
	}
	M1000_Device *dev = (M1000_Device *) t->user_data;
	dev->in_completion(t);
}

void M1000_Device::in_completion(libusb_transfer *t)
{
	std::lock_guard<std::mutex> lock(m_state);
	m_in_transfers.num_active--;

	if (t->status == LIBUSB_TRANSFER_COMPLETED) {
		// Store exceptions to rethrow them in the main thread in read()/write().
		try {
			handle_in_transfer(t);
		} catch (...) {
			e_ptr = std::current_exception();
		}

		if (!m_session->cancelled()) {
			submit_in_transfer(t);
		}
	} else if (t->status != LIBUSB_TRANSFER_CANCELLED) {
		m_session->handle_error(t->status, "M1000_Device::in_completion");
	}
	if (m_out_transfers.num_active == 0 && m_in_transfers.num_active == 0) {
		m_session->completion();
	}
}

// Runs in USB thread
extern "C" void LIBUSB_CALL m1000_out_completion(libusb_transfer *t)
{
	// if the user_data field is empty, something's wrong, but we have a completed transfer, so free it
	if (!t->user_data) {
		libusb_free_transfer(t);
		return;
	}
	M1000_Device *dev = (M1000_Device *) t->user_data;
	dev->out_completion(t);
}

void M1000_Device::out_completion(libusb_transfer *t)
{
	std::lock_guard<std::mutex> lock(m_state);
	m_out_transfers.num_active--;

	if (t->status == LIBUSB_TRANSFER_COMPLETED) {
		// Store exceptions to rethrow them in the main thread in read()/write().
		try {
			handle_out_transfer(t);
		} catch (...) {
			e_ptr = std::current_exception();
		}

		if (!m_session->cancelled()) {
			submit_out_transfer(t);
		}
	} else if (t->status != LIBUSB_TRANSFER_CANCELLED) {
		 m_session->handle_error(t->status, "M1000_Device::out_completion");
	}
	if (m_out_transfers.num_active == 0 && m_in_transfers.num_active == 0) {
		m_session->completion();
	}
}

int M1000_Device::configure(uint64_t sampleRate)
{
	int ret;

	if (!sampleRate)
		sampleRate = get_default_rate();

	double sample_time = 1.0 / sampleRate;
	double M1K_timer_clock;

	// If firmware version is 023314a, initial production, use 3e6 for timer clock
	// otherwise, assume a more recent firmware, and use a faster clock.
	if (strcmp(m_fw_version, "023314a*") == 0) {
		M1K_timer_clock = 3e6;
	} else {
		M1K_timer_clock = 48e6;
	}

	m_sam_per = round(sample_time * M1K_timer_clock) / 2;
	if (m_sam_per < m_min_per)
		m_sam_per = m_min_per;
	sample_time = m_sam_per / M1K_timer_clock; // convert back to get the actual sample time;

	unsigned transfers = 8;
	m_packets_per_transfer = ceil(BUFFER_TIME / (sample_time * chunk_size) / transfers);

	ret = m_in_transfers.alloc(transfers, m_usb, EP_IN, LIBUSB_TRANSFER_TYPE_BULK,
		m_packets_per_transfer * in_packet_size, 10000, m1000_in_completion, this);
	if (ret)
		return ret;
	ret = m_out_transfers.alloc(transfers, m_usb, EP_OUT, LIBUSB_TRANSFER_TYPE_BULK,
		m_packets_per_transfer * out_packet_size, 10000, m1000_out_completion, this);
	m_in_transfers.num_active = m_out_transfers.num_active = 0;
	return ret;
}

// Constrain a given value by low and high bounds.
static float constrain(float val, float lo, float hi)
{
	if (val > hi) val = hi;
	if (val < lo) val = lo;
	return val;
}

uint16_t M1000_Device::encode_out(unsigned channel)
{
	float val;
	int v = 32768 * 4 / 5;

	if (m_mode[channel] != DISABLED)
		while (!m_out_samples_q[channel]->pop(val));

	if (m_mode[channel] == SVMI) {
		val = (val - m_cal.offset[channel*4+2]) * m_cal.gain_p[channel*4+2];
		val = constrain(val, m_signals[channel][0].info()->min, m_signals[channel][0].info()->max);
		v = val * (1/m_signals[channel][0].info()->resolution);
	} else if (m_mode[channel] == SIMV) {
		if (val > 0) {
			val = (val - m_cal.offset[channel*4+3]) * m_cal.gain_p[channel*4+3];
		}
		else {
			val = (val - m_cal.offset[channel*4+3]) * m_cal.gain_n[channel*4+3];
		}
		val = constrain(val, m_signals[channel][1].info()->min, m_signals[channel][1].info()->max);
		v = 65536*(2./5. + 0.8*0.2*20.*0.5*val);
	}
	v = constrain(v, 0, 65535);
	return v;
}

void M1000_Device::handle_out_transfer(libusb_transfer* t)
{
	for (unsigned p = 0; p < m_packets_per_transfer; p++) {
		uint8_t* buf = (uint8_t*) (t->buffer + p * out_packet_size);
		for (unsigned i = 0; i < chunk_size; i++) {
			if (strncmp(m_fw_version, "2.", 2) == 0) {
				uint16_t a = encode_out(CHAN_A);
				buf[i*4+0] = a >> 8;
				buf[i*4+1] = a & 0xff;
				uint16_t b = encode_out(CHAN_B);
				buf[i*4+2] = b >> 8;
				buf[i*4+3] = b & 0xff;
			} else {
				uint16_t a = encode_out(CHAN_A);
				buf[(i+chunk_size*0)*2]   = a >> 8;
				buf[(i+chunk_size*0)*2+1] = a & 0xff;
				uint16_t b = encode_out(CHAN_B);
				buf[(i+chunk_size*1)*2]   = b >> 8;
				buf[(i+chunk_size*1)*2+1] = b & 0xff;
			}
			m_out_sampleno++;
		}
	}
}

int M1000_Device::submit_out_transfer(libusb_transfer* t)
{
	int ret;
	if (m_sample_count == 0 || m_out_sampleno < m_sample_count) {
		ret = libusb_submit_transfer(t);
		if (ret != 0) {
			m_out_transfers.failed(t);
			m_session->handle_error(ret, "M1000_Device::submit_out_transfer");
			return ret;
		}
		m_out_transfers.num_active++;
		return 0;
	}
	return -1;
}

int M1000_Device::submit_in_transfer(libusb_transfer* t)
{
	int ret;
	if (m_sample_count == 0 || m_requested_sampleno < m_sample_count) {
		ret = libusb_submit_transfer(t);
		if (ret != 0) {
			m_in_transfers.failed(t);
			m_session->handle_error(ret, "M1000_Device::submit_in_transfer");
			return ret;
		}
		m_in_transfers.num_active++;
		m_requested_sampleno += m_packets_per_transfer * IN_SAMPLES_PER_PACKET;
		return 0;
	}
	return -1;
}

ssize_t M1000_Device::read(std::vector<std::array<float, 4>>& buf, size_t samples, int timeout)
{
	std::array<float, 4> sample;
	bool succeeded;
	buf.clear();

	// Initialize sample acquiring duration check for timeout support.
	auto clk_start = std::chrono::high_resolution_clock::now();

	for (unsigned i = 0; i < samples; i++) {
		do {
			succeeded = m_in_samples_q.pop(sample);
			// stop waiting for samples if we've run out of time
			auto clk_end = std::chrono::high_resolution_clock::now();
			auto clk_diff = std::chrono::duration_cast<std::chrono::milliseconds>(clk_end - clk_start);
			if (timeout >= 0 && clk_diff.count() > timeout)
				break;
			std::this_thread::sleep_for(std::chrono::nanoseconds(10));
		} while (!succeeded);

		if (succeeded)
			buf.push_back(sample);
	}

	// If a data overflow occurred in the USB thread, rethrow the exception
	// here in the main thread. This allows users to just wrap read() in order
	// to catch and/or act on overflows.
	if (e_ptr) {
		std::rethrow_exception(e_ptr);
	}

	return buf.size();
}

// Queue up samples being sent to the device.
static void push_samples(boost::lockfree::spsc_queue<float>& q, std::vector<float>& buf)
{
	for (auto x: buf)
		while (!q.push(x));
}

int M1000_Device::write(std::vector<float>& buf, unsigned channel)
{
	// bad channel
	if (channel != 0 && channel != 1)
		return -ENODEV;

	// finish the last write() call
	if (m_out_samples_thr[channel].joinable())
		m_out_samples_thr[channel].join();

	std::thread t(push_samples, std::ref(*m_out_samples_q[channel]), std::ref(buf));
	std::swap(t, m_out_samples_thr[channel]);

	// If a data underflow occurred in the USB thread, rethrow the exception
	// here in the main thread. This allows users to just wrap write() in order
	// to catch and/or act on underflows.
	if (e_ptr) {
		std::rethrow_exception(e_ptr);
	}

	return 0;
}

void M1000_Device::handle_in_transfer(libusb_transfer* t)
{
	float v;
	std::array<float, 4> samples;

	for (unsigned p = 0; p < m_packets_per_transfer; p++) {
		uint8_t* buf = (uint8_t*) (t->buffer + p * in_packet_size);

		for (unsigned i = 0; i < chunk_size; i++) {
			// M1K firmware versions >= 2.00 use an interleaved data format.
			if (strncmp(m_fw_version, "2.", 2) == 0) {
				v = (buf[i*8+0] << 8 | buf[i*8+1]) * m_signals[0][0].info()->resolution;
				samples[0] = (v - m_cal.offset[0]) * m_cal.gain_p[0];
				v = (((buf[i*8+2] << 8 | buf[i*8+3]) * m_signals[0][1].info()->resolution) - 0.195)*1.25;
				samples[1] = (v - m_cal.offset[1]) * (samples[1] > 0 ? m_cal.gain_p[1] : m_cal.gain_n[1]);
				v = (buf[i*8+4] << 8 | buf[i*8+5]) * m_signals[1][0].info()->resolution;
				samples[2] = (v - m_cal.offset[4]) * m_cal.gain_p[4];
				v = (((buf[i*8+6] << 8 | buf[i*8+7]) * m_signals[1][1].info()->resolution) - 0.195)*1.25;
				samples[3] = (v - m_cal.offset[5]) * (samples[3] > 0 ? m_cal.gain_p[5] : m_cal.gain_n[5]);
			} else {
				v = (buf[(i+chunk_size*0)*2] << 8 | buf[(i+chunk_size*0)*2+1]) * m_signals[0][0].info()->resolution;
				samples[0] = (v - m_cal.offset[0]) * m_cal.gain_p[0];
				v = (((buf[(i+chunk_size*1)*2] << 8 | buf[(i+chunk_size*1)*2+1]) * m_signals[0][1].info()->resolution) - 0.195)*1.25;
				samples[1] = (v - m_cal.offset[1]) * (samples[1] > 0 ? m_cal.gain_p[1] : m_cal.gain_n[1]);
				v = (buf[(i+chunk_size*2)*2] << 8 | buf[(i+chunk_size*2)*2+1]) * m_signals[1][0].info()->resolution;
				samples[2] = (v - m_cal.offset[4]) * m_cal.gain_p[4];
				v = (((buf[(i+chunk_size*3)*2] << 8 | buf[(i+chunk_size*3)*2+1]) * m_signals[1][1].info()->resolution) - 0.195)*1.25;
				samples[3] = (v - m_cal.offset[5]) * (samples[3] > 0 ? m_cal.gain_p[5] : m_cal.gain_n[5]);
			}
			m_in_sampleno++;
			while (!m_in_samples_q.push(samples));
		}
	}
}

const sl_device_info* M1000_Device::info() const
{
	return &m1000_info;
}

const sl_channel_info* M1000_Device::channel_info(unsigned channel) const
{
	if (channel < 2) {
		return &m1000_channel_info[channel];
	} else {
		return NULL;
	}
}

Signal* M1000_Device::signal(unsigned channel, unsigned signal)
{
	if (channel < 2 && signal < 2) {
		return &m_signals[channel][signal];
	} else {
		return NULL;
	}
}

int M1000_Device::set_mode(unsigned channel, unsigned mode)
{
	int ret = 0;

	// bad channel
	if (channel != 0 && channel != 1)
		return -ENODEV;

	m_mode[channel] = mode;

	// set feedback potentiometers with mode heuristics
	unsigned pset;
	switch (mode) {
		case SIMV: pset = 0x7f7f; break;
		case SVMI: pset = 0x0000; break;
		case DISABLED:
		default: pset = 0x3000;
	};

	ret = ctrl_transfer(0x40, 0x59, channel, pset, 0, 0, 100);
	if (ret < 0)
		return -libusb_to_errno(ret);

	// set mode
	ret = ctrl_transfer(0x40, 0x53, channel, mode, 0, 0, 100);
	if (ret < 0) {
		ret = -libusb_to_errno(ret);
	} else {
		// We don't care about the number of bytes transferred so reset to 0 if successful.
		ret = 0;
	}
	return ret;
}

int M1000_Device::fwver_sem(std::array<unsigned, 3>& components)
{
	components = {0, 0, 0};
	std::vector<std::string> split_version;

	boost::split(split_version, m_fw_version, boost::is_any_of("."), boost::token_compress_on);
	try {
		components[0] = atoi(split_version.at(0).c_str());
		components[1] = atoi(split_version.at(1).c_str());
		components[2] = atoi(split_version.at(2).c_str());
	} catch (const std::out_of_range) {
		// ignore missing version portions
	}

	return 0;
}

int M1000_Device::on()
{
	int ret = 0;
	ret = libusb_set_interface_alt_setting(m_usb, 0, 1);
	if (ret < 0)
		return -libusb_to_errno(ret);

	ret = ctrl_transfer(0x40, 0xC5, 0, 0, 0, 0, 100);
	if (ret < 0)
		return -libusb_to_errno(ret);
	ret = ctrl_transfer(0x40, 0xCC, 0, 0, 0, 0, 100);
	if (ret < 0) {
		ret = -libusb_to_errno(ret);
	} else {
		// We don't care about the number of bytes transferred so reset to 0 if successful.
		ret = 0;
	}
	return ret;
}

int M1000_Device::sync()
{
	int ret = 0;
	ret = ctrl_transfer(0xC0, 0x6F, 0, 0, (unsigned char*)&m_sof_start, 2, 100);
	m_sof_start = (m_sof_start + 0xff) & 0x3c00;

	if (ret < 0) {
		ret = -libusb_to_errno(ret);
	} else {
		// We don't care about the number of bytes transferred so reset to 0 if successful.
		ret = 0;
	}
	return ret;
}

int M1000_Device::run(uint64_t samples)
{
	int ret = ctrl_transfer(0x40, 0xC5, m_sam_per, m_sof_start, 0, 0, 100);
	if (ret < 0) {
		return -libusb_to_errno(ret);
	}
	std::lock_guard<std::mutex> lock(m_state);
	m_sample_count = samples;
	m_requested_sampleno = m_in_sampleno = m_out_sampleno = 0;

	for (auto i: m_in_transfers) {
		if (submit_in_transfer(i)) break;
	}

	for (auto i: m_out_transfers) {
		if (submit_out_transfer(i)) break;
	}
	return 0;
}

int M1000_Device::cancel()
{
	int ret_in = m_in_transfers.cancel();
	int ret_out = m_out_transfers.cancel();
	if ((ret_in != ret_out) || (ret_in != 0) || (ret_out != 0))
		return -1;
	return 0;
}

int M1000_Device::off()
{
	int ret = 0;
	ret = set_mode(CHAN_A, DISABLED);
	if (ret < 0)
		return ret;
	ret = set_mode(CHAN_B, DISABLED);
	if (ret < 0)
		return ret;

	ret = ctrl_transfer(0x40, 0xC5, 0, 0, 0, 0, 100);
	if (ret < 0) {
		ret = -libusb_to_errno(ret);
	} else {
		// We don't care about the number of bytes transferred so reset to 0 if successful.
		ret = 0;
	}
	return ret;
}

int M1000_Device::samba_mode()
{
	int ret = 0;

	ret = ctrl_transfer(0x40, 0xbb, 0, 0, NULL, 0, 500);
	// Wait for 1 second for the device to drop into SAM-BA bootloader mode.
	// Without a delay often the code scanning the system for device signatures
	// matching SAM-BA mode won't find anything because the device hasn't fully
	// switched over yet and been re-enumerated by the host system.
	std::this_thread::sleep_for(std::chrono::seconds(1));
	if (ret < 0 && (ret != LIBUSB_ERROR_IO && ret != LIBUSB_ERROR_PIPE))
		return -libusb_to_errno(ret);
	return 0;
}
