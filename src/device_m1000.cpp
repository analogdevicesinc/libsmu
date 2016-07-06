// Released under the terms of the BSD License
// (C) 2014-2016
//   Analog Devices, Inc.
//   Kevin Mehall <km@kevinmehall.net>
//   Ian Daniher <itdaniher@gmail.com>

#include "device_m1000.hpp"

#include <cmath>
#include <cstring>
#include <vector>

#include <libusb.h>

#include "debug.hpp"
#include "transfers.hpp"
#include <libsmu/libsmu.hpp>

#define EP_OUT 0x02
#define EP_IN 0x81

const unsigned chunk_size = 256;
#define OUT_SAMPLES_PER_PACKET chunk_size
#define IN_SAMPLES_PER_PACKET chunk_size
#define A 0
#define B 1

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

using namespace smu;

static const sl_device_info m1000_info = {DEVICE_M1000, "ADALM1000", 2};

static const sl_channel_info m1000_channel_info[2] = {
	{CHANNEL_SMU, "A", 3, 2},
	{CHANNEL_SMU, "B", 3, 2},
};

const float current_limit = 0.2;

int M1000_Device::get_default_rate()
{
	// rev0 firmware
	if (strcmp(this->m_fw_version, "023314a*") == 0) {
		return 62500;
	}
	// modern fw
	else {
		return 100000;
	}
}

int M1000_Device::init()
{
	int d = Device::init();

	if (d != 0) {
		return d;
	} else {
		return 0;
	}
}

int M1000_Device::added()
{
	libusb_claim_interface(m_usb, 0);
	read_calibration();
	return 0;
}

int M1000_Device::removed()
{
	libusb_release_interface(m_usb, 0);
	return 0;
}

void M1000_Device::read_calibration()
{
	int ret;

	ret = this->ctrl_transfer(0xC0, 0x01, 0, 0, (unsigned char*)&m_cal, sizeof(EEPROM_cal), 100);
	if(ret <= 0 || m_cal.eeprom_valid != EEPROM_VALID) {
		for(int i = 0; i < 8; i++) {
			m_cal.offset[i] = 0.0f;
			m_cal.gain_p[i] = 1.0f;
			m_cal.gain_n[i] = 1.0f;
		}
	}
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
		for(int i = 0; i < 8; i++) {
			m_cal.offset[i] = 0.0f;
			m_cal.gain_p[i] = 1.0f;
			m_cal.gain_n[i] = 1.0f;
		}
		cal_records_no = 8;
		goto write_cal;
	}

	fp = fopen(cal_file_name, "r");
	if(!fp) {
		return -1;
	}

	while(fgets(str, 128, fp) != NULL) {
		if(strstr(str, "</>")) {
			rec_idx = 0;
			while(fgets(str, 128, fp) != NULL) {
				if(strstr(str, "<\\>") && rec_idx) {
					gain_p = 0;
					gain_n = 0;
					cnt_n = 0;
					cnt_p = 0;
					m_cal.offset[cal_records_no] = val[0] - ref[0];
					for(int i = 1; i < rec_idx; i++) {
						if(ref[i] > 0) {
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
	if(cal_records_no != 8) {
		// invalid calibration file
		ret = -EINVAL;
	} else {
		m_cal.eeprom_valid = EEPROM_VALID;
		ret = this->ctrl_transfer(0x40, 0x02, 0, 0, (unsigned char*)&m_cal, sizeof(EEPROM_cal), 100);
		if (ret > 0)
			ret = 0;
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
		handle_in_transfer(t);
		// m_cancellation == 0, everything OK
		if (m_session->m_cancellation == 0) {
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
		if (m_session->m_cancellation == 0) {
			submit_out_transfer(t);
		}
	} else if (t->status != LIBUSB_TRANSFER_CANCELLED) {
		 m_session->handle_error(t->status, "M1000_Device::out_completion");
	}
	if (m_out_transfers.num_active == 0 && m_in_transfers.num_active == 0) {
		m_session->completion();
	}
}

void M1000_Device::configure(uint64_t sampleRate)
{
	double sample_time = 1.0 / sampleRate;
	double M1K_timer_clock;

	// if FW version is 023314a - initial production, use 3e6 for timer clock
	// otherwise, assume a more recent firmware, and use the audacious clock
	if (strcmp(this->m_fw_version, "023314a*") == 0) {
		M1K_timer_clock = 3e6;
	} else {
		M1K_timer_clock = 48e6;
	}

	m_sam_per = round(sample_time * M1K_timer_clock) / 2;
	if (m_sam_per < m_min_per) m_sam_per = m_min_per;
	sample_time = m_sam_per / M1K_timer_clock; // convert back to get the actual sample time;

	unsigned transfers = 8;
	m_packets_per_transfer = ceil(BUFFER_TIME / (sample_time * chunk_size) / transfers);

	m_in_transfers.alloc(transfers, m_usb, EP_IN,LIBUSB_TRANSFER_TYPE_BULK,
		m_packets_per_transfer*in_packet_size,10000, m1000_in_completion, this);
	m_out_transfers.alloc(transfers, m_usb, EP_OUT, LIBUSB_TRANSFER_TYPE_BULK,
		m_packets_per_transfer*out_packet_size, 10000, m1000_out_completion, this);
	m_in_transfers.num_active = m_out_transfers.num_active = 0;
}

uint16_t M1000_Device::encode_out(unsigned chan)
{
	int v = 0;
	if (m_mode[chan] == SVMI) {
		float val = m_signals[chan][0].get_sample();
		val = (val - m_cal.offset[chan*4+2]) * m_cal.gain_p[chan*4+2];
		val = constrain(val, 0, 5.0);
		v = 65535*val/5.0;
	} else if (m_mode[chan] == SIMV) {
		float val = m_signals[chan][1].get_sample();
		if(val > 0) {
			val = (val - m_cal.offset[chan*4+3]) * m_cal.gain_p[chan*4+3];
		}
		else {
			val = (val - m_cal.offset[chan*4+3]) * m_cal.gain_n[chan*4+3];
		}
		val = constrain(val, -current_limit, current_limit);
		v = 65536*(2./5. + 0.8*0.2*20.*0.5*val);
	} else if (m_mode[chan] == DISABLED) {
		v = 32768*4/5;
	}
	if (v > 65535) v = 65535;
	if (v < 0) v = 0;
	return v;
}

bool M1000_Device::submit_out_transfer(libusb_transfer* t)
{
	if (m_sample_count == 0 || m_out_sampleno < m_sample_count) {
		for (unsigned p = 0; p < m_packets_per_transfer; p++) {
			uint8_t* buf = (uint8_t*) (t->buffer + p * out_packet_size);
			for (unsigned i = 0; i < chunk_size; i++) {
				if (strncmp(this->m_fw_version, "2.", 2) == 0) {
					uint16_t a = encode_out(0);
					buf[i*4+0] = a >> 8;
					buf[i*4+1] = a & 0xff;
					uint16_t b = encode_out(1);
					buf[i*4+2] = b >> 8;
					buf[i*4+3] = b & 0xff;
				} else {
					uint16_t a = encode_out(0);
					buf[(i+chunk_size*0)*2	] = a >> 8;
					buf[(i+chunk_size*0)*2+1] = a & 0xff;
					uint16_t b = encode_out(1);
					buf[(i+chunk_size*1)*2	] = b >> 8;
					buf[(i+chunk_size*1)*2+1] = b & 0xff;
				}
				m_out_sampleno++;
			}
		}
		int r = libusb_submit_transfer(t);
		if (r != 0) {
			m_out_transfers.failed(t);
			// writes to t->status is illegal
			// t->status = (libusb_transfer_status) r;
			m_session->handle_error(r, "M1000_Device::submit_out_transfer");
			return false;
		}
		m_out_transfers.num_active++;
		return true;
	}
	return false;
}


bool M1000_Device::submit_in_transfer(libusb_transfer* t)
{
	if (m_sample_count == 0 || m_requested_sampleno < m_sample_count) {
		int r = libusb_submit_transfer(t);
		if (r != 0) {
			m_in_transfers.failed(t);
			//t->status = (libusb_transfer_status) r;
			m_session->handle_error(r, "M1000_Device::submit_in_transfer");
			return false;
		}
		m_in_transfers.num_active++;
		m_requested_sampleno += m_packets_per_transfer*IN_SAMPLES_PER_PACKET;
		return true;
	}
	return false;
}

void M1000_Device::handle_in_transfer(libusb_transfer* t)
{
	float val;
	for (unsigned p = 0; p < m_packets_per_transfer; p++) {
		uint8_t* buf = (uint8_t*) (t->buffer + p * in_packet_size);

		for (unsigned i = 0; i < chunk_size; i++) {
			if (strncmp(this->m_fw_version, "2.", 2) == 0) {
				val = (buf[i*8+0] << 8 | buf[i*8+1]) / 65535.0 * 5.0;
				m_signals[0][0].put_sample((val - m_cal.offset[0]) * m_cal.gain_p[0]);
				val = (((buf[i*8+2] << 8 | buf[i*8+3]) / 65535.0 * 0.4 ) - 0.195)*1.25;
				m_signals[0][1].put_sample((val - m_cal.offset[1]) * (val > 0 ? m_cal.gain_p[1] : m_cal.gain_n[1]));
				val = (buf[i*8+4] << 8 | buf[i*8+5]) / 65535.0 * 5.0;
				m_signals[1][0].put_sample((val - m_cal.offset[4]) * m_cal.gain_p[4]);
				val = (((buf[i*8+6] << 8 | buf[i*8+7]) / 65535.0 * 0.4 ) - 0.195)*1.25;
				m_signals[1][1].put_sample((val - m_cal.offset[5]) * (val > 0 ? m_cal.gain_p[5] : m_cal.gain_n[5]));
			} else {
				val = (buf[(i+chunk_size*0)*2] << 8 | buf[(i+chunk_size*0)*2+1]) / 65535.0 * 5.0;
				m_signals[0][0].put_sample((val - m_cal.offset[0]) * m_cal.gain_p[0]);
				val = (((buf[(i+chunk_size*1)*2] << 8 | buf[(i+chunk_size*1)*2+1]) / 65535.0 * 0.4 ) - 0.195)*1.25;
				m_signals[0][1].put_sample((val - m_cal.offset[0]) * (val > 0 ? m_cal.gain_p[1] : m_cal.gain_n[1]));
				val = (buf[(i+chunk_size*2)*2] << 8 | buf[(i+chunk_size*2)*2+1]) / 65535.0 * 5.0;
				m_signals[1][0].put_sample((val - m_cal.offset[4]) * m_cal.gain_p[4]);
				val = (((buf[(i+chunk_size*3)*2] << 8 | buf[(i+chunk_size*3)*2+1]) / 65535.0 * 0.4) - 0.195)*1.25;
				m_signals[1][1].put_sample((val - m_cal.offset[5]) * (val > 0 ? m_cal.gain_p[5] : m_cal.gain_n[5]));
			}
			m_in_sampleno++;
		}
	}

	m_session->progress();
}

// get device info struct
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

void M1000_Device::set_mode(unsigned chan, unsigned mode)
{
	if (chan < 2) {
		m_mode[chan] = mode;
	}
	// set feedback potentiometers with mode heuristics
	unsigned pset;
	switch (mode) {
		case SIMV: pset = 0x7f7f; break;
		case SVMI: pset = 0x0000; break;
		case DISABLED:
		default: pset = 0x3000;
	};
	this->ctrl_transfer(0x40, 0x59, chan, pset, 0, 0, 100);
	// set mode
	this->ctrl_transfer(0x40, 0x53, chan, mode, 0, 0, 100);
}

void M1000_Device::on()
{
	libusb_set_interface_alt_setting(m_usb, 0, 1);

	this->ctrl_transfer(0x40, 0xC5, 0, 0, 0, 0, 100);
	this->ctrl_transfer(0x40, 0xCC, 0, 0, 0, 0, 100);
}

void M1000_Device::sync()
{
	this->ctrl_transfer(0xC0, 0x6F, 0, 0, (unsigned char*)&m_sof_start, 2, 100);
	m_sof_start = (m_sof_start+0xff)&0x3c00;
}

void M1000_Device::start_run(uint64_t samples)
{
	int ret = this->ctrl_transfer(0x40, 0xC5, m_sam_per, m_sof_start, 0, 0, 100);
	if (ret < 0) {
		smu_debug("control transfer failed with code %i\n", ret);
		return;
	}
	std::lock_guard<std::mutex> lock(m_state);
	m_sample_count = samples;
	m_requested_sampleno = m_in_sampleno = m_out_sampleno = 0;

	for (auto i: m_in_transfers) {
		if (submit_in_transfer(i) != 0) break;
	}

	for (auto i: m_out_transfers) {
		if (submit_out_transfer(i) != 0) break;
	}
}

void M1000_Device::cancel()
{
	int ret_in = m_in_transfers.cancel();
	int ret_out = m_out_transfers.cancel();
	if ((ret_in != ret_out) || (ret_in != 0) || (ret_out != 0))
		smu_debug("cancel error in: %s out: %s\n", libusb_error_name(ret_in), libusb_error_name(ret_out));
}

void M1000_Device::off()
{
	set_mode(A, DISABLED);
	set_mode(B, DISABLED);
	this->ctrl_transfer(0x40, 0xC5, 0, 0, 0, 0, 100);
}

// Force the device into SAM-BA command mode.
void M1000_Device::samba_mode()
{
	int ret;

	ret = this->ctrl_transfer(0x40, 0xbb, 0, 0, NULL, 0, 500);
	std::this_thread::sleep_for(std::chrono::seconds(1));
	if (ret < 0 && (ret != LIBUSB_ERROR_IO && ret != LIBUSB_ERROR_PIPE)) {
		std::string libusb_error_str(libusb_strerror((enum libusb_error)ret));
		throw std::runtime_error("failed to enable SAM-BA command mode: " + libusb_error_str);
	}
}
