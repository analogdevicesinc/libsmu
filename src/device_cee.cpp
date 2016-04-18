// Released under the terms of the BSD License
// (C) 2012-2015
//   Kevin Mehall <km@kevinmehall.net>
//   Ian Daniher <itdaniher@gmail.com>

#include "device_cee.hpp"
#include <libusb.h>
#include <iostream>
#include <cstring>
#include <cmath>

#define EP_IN 0x81
#define EP_OUT 0x02

#define CMD_CONFIG_CAPTURE 0x80
#define CMD_CONFIG_GAIN 0x65
#define CMD_ISET_DAC 0x15
#define DEVMODE_OFF  0
#define DEVMODE_2SMU 1

const int CEE_timer_clock = 4e6; // 4 MHz
const double CEE_current_gain_scale = 100000;
const uint32_t CEE_default_current_gain = 45*.07*CEE_current_gain_scale;

const int default_current_limit = 200;

#ifdef _WIN32
const double BUFFER_TIME = 0.050;
#else
const double BUFFER_TIME = 0.020;
#endif

static const sl_device_info cee_info = {DEVICE_CEE_1, "CEE", 2};

static const sl_channel_info cee_channel_info[2] = {
	{CHANNEL_SMU, "A", 3, 2},
	{CHANNEL_SMU, "B", 3, 2},
};

// Mode 0: high-z
// Mode 1: SVMI
// Mode 2: SIMV

static const sl_signal_info cee_signal_info[2] = {
	{ SIGNAL, "Voltage", 0x7, 0x2, unit_V,  0.0, 5.0, 5.0/4095 },
	{ SIGNAL, "Current", 0x6, 0x4, unit_A, -0.2, 0.2, 0.4/4095 },
};

inline int16_t signextend12(uint16_t v) {
	return (v>((1<<11)-1))?(v - (1<<12)):v;
}

struct IN_sample {
	uint8_t avl, ail, aih_avh, bvl, bil, bih_bvh;

	int16_t av() { return signextend12((aih_avh&0x0f)<<8) | avl; }
	int16_t bv() { return signextend12((bih_bvh&0x0f)<<8) | bvl; }
	int16_t ai() { return signextend12((aih_avh&0xf0)<<4) | ail; }
	int16_t bi() { return signextend12((bih_bvh&0xf0)<<4) | bil; }
} __packed;

#define IN_SAMPLES_PER_PACKET 10
#define FLAG_PACKET_DROPPED (1<<0)

typedef struct IN_packet {
	uint8_t mode_a;
	uint8_t mode_b;
	uint8_t flags;
	uint8_t mode_seq;
	IN_sample data[10];
} __packed IN_packet;

#define OUT_SAMPLES_PER_PACKET 10
struct OUT_sample {
	uint8_t al, bl, bh_ah;
	void pack(uint16_t a, uint16_t b) {
		al = a & 0xff;
		bl = b & 0xff;
		bh_ah = ((b>>4)&0xf0) |	(a>>8);
	}
} __packed;

typedef struct OUT_packet {
	uint8_t mode_a;
	uint8_t mode_b;
	OUT_sample data[10];
} __packed OUT_packet;

#define EEPROM_VALID_MAGIC 0x90e26cee
#define EEPROM_FLAG_USB_POWER (1<<0)

typedef struct CEE_version_descriptor {
	uint8_t version_major;
	uint8_t version_minor;
	uint8_t flags;
	uint8_t per_ns;
	uint8_t min_per;
} __packed CEE_version_descriptor;

CEE_Device::CEE_Device(Session* s, libusb_device* device):
	Device(s, device),
	m_signals {
		{Signal(&cee_signal_info[0]), Signal(&cee_signal_info[1])},
		{Signal(&cee_signal_info[0]), Signal(&cee_signal_info[1])},
	},
	m_mode{0,0}
{	}

CEE_Device::~CEE_Device() {}

int CEE_Device::get_default_rate() {
	return 80000;
}


int CEE_Device::init() {
	int r = Device::init();
	if (r!=0) { return r; }

	char buf[64];
	r = libusb_control_transfer(m_usb, 0xC0, 0x00, 0, 0, (uint8_t*)buf, 64, 100);
	if (r >= 0) {
		m_hw_version = std::string(buf, strnlen(buf, r));
	}

	r = libusb_control_transfer(m_usb, 0xC0, 0x00, 0, 1, (uint8_t*)buf, 64, 100);
	if (r >= 0) {
		m_fw_version = std::string(buf, strnlen(buf, r));
	}

	CEE_version_descriptor version_info;
	bool have_version_info = 0;

	if (m_fw_version >= "1.2") {
		r = libusb_control_transfer(m_usb, 0xC0, 0x00, 0, 0xff, (uint8_t*)&version_info, sizeof(version_info), 100);
		have_version_info = (r>=0);

		r = libusb_control_transfer(m_usb, 0xC0, 0x00, 0, 2, (uint8_t*)buf, 64, 100);
		if (r >= 0) {
			m_git_version = std::string(buf, strnlen(buf, r));
		}
	}

	if (have_version_info) {
		m_min_per = version_info.min_per;
		if (version_info.per_ns != 250) {
			smu_debug("Error: alternate timer clock %i is not supported in this release.\n", version_info.per_ns);
		}

	} else {
		m_min_per = 100;
	}

	smu_debug("Hardware: %s\n", m_hw_version.c_str());
	smu_debug("Firmware version: %s ( %s )\n", m_fw_version.c_str(), m_git_version.c_str());
	smu_debug("Supported sample rate: %f ksps\n", (double) CEE_timer_clock / (double) m_min_per / 1000.0);
	return 0;
}


int CEE_Device::added() {
	libusb_claim_interface(m_usb, 0);

	// Reset the state
	libusb_control_transfer(m_usb, 0x40, CMD_CONFIG_CAPTURE, 0, DEVMODE_OFF, 0, 0, 100);

	// Reset the gains
	libusb_control_transfer(m_usb, 0x40, CMD_CONFIG_GAIN, (0x01<<2), 0, 0, 0, 100);
	libusb_control_transfer(m_usb, 0x40, CMD_CONFIG_GAIN, (0x00<<2), 1, 0, 0, 100);
	libusb_control_transfer(m_usb, 0x40, CMD_CONFIG_GAIN, (0x00<<2), 2, 0, 0, 100);
	libusb_control_transfer(m_usb, 0x40, CMD_CONFIG_GAIN, (0x01<<2), 3, 0, 0, 100);

	read_calibration();

	return 0;
}

int CEE_Device::removed() {
	libusb_release_interface(m_usb, 0);
	return 0;
}

void CEE_Device::read_calibration() {
	union {
		uint8_t buf[64];
		uint32_t magic;
	};
	int r = libusb_control_transfer(m_usb, 0xC0, 0xE0, 0, 0, buf, 64, 100);
	if (!r || magic != EEPROM_VALID_MAGIC) {
		smu_debug("Reading calibration data failed: %i\n", r);
		memset(&m_cal, 0xff, sizeof(m_cal));

		m_cal.offset_a_v = m_cal.offset_a_i = m_cal.offset_b_v = m_cal.offset_b_i = 0;
		m_cal.dac400_a = m_cal.dac400_b = m_cal.dac200_a = m_cal.dac200_b = 0x6B7;
	} else {
		memcpy((uint8_t*)&m_cal, buf, sizeof(m_cal));
	}

	set_current_limit((m_cal.flags&EEPROM_FLAG_USB_POWER)?default_current_limit:2000);

	if (m_cal.current_gain_a == (uint32_t) -1) {
		m_cal.current_gain_a = CEE_default_current_gain;
	}

	if (m_cal.current_gain_b == (uint32_t) -1) {
		m_cal.current_gain_b = CEE_default_current_gain;
	}

	smu_debug("Current gain %i %i\n", m_cal.current_gain_a, m_cal.current_gain_b);
}

void CEE_Device::set_current_limit(unsigned mode) {
	unsigned ilimit_cal_a, ilimit_cal_b;

	if (mode == 200) {
		ilimit_cal_a = m_cal.dac200_a;
		ilimit_cal_b = m_cal.dac200_b;
	} else if(mode == 400) {
		ilimit_cal_a = m_cal.dac400_a;
		ilimit_cal_b = m_cal.dac400_b;
	} else if (mode == 2000) {
		ilimit_cal_a = ilimit_cal_b = 0;
	} else {
		smu_debug("Invalid current limit: %u\n", mode);
		return;
	}

	libusb_control_transfer(m_usb, 0xC0, CMD_ISET_DAC, ilimit_cal_a, ilimit_cal_b, NULL, 0, 100);
	m_current_limit = mode;
}

/// Runs in USB thread
extern "C" void LIBUSB_CALL cee_in_completion(libusb_transfer *t) {
	if (!t->user_data) {
		libusb_free_transfer(t); // user_data was zeroed out when device was deleted
		return;
	}

	CEE_Device *dev = (CEE_Device *) t->user_data;
	dev->in_completion(t);
}

void CEE_Device::in_completion(libusb_transfer* t) {
	std::lock_guard<std::mutex> lock(m_state);

	m_in_transfers.num_active--;

	if (t->status == LIBUSB_TRANSFER_COMPLETED) {
		handle_in_transfer(t);
		if (m_session->m_cancellation == 0) {
			submit_in_transfer(t);
		}
	} else if (t->status != LIBUSB_TRANSFER_CANCELLED) {
		m_session->handle_error(t->status, "CEE_Device::in_completion");
	}

	if (m_out_transfers.num_active == 0 && m_in_transfers.num_active == 0) {
		m_session->completion();
	}
}

/// Runs in USB thread
extern "C" void LIBUSB_CALL cee_out_completion(libusb_transfer *t) {
	if (!t->user_data) {
		libusb_free_transfer(t); // user_data was zeroed out when device was deleted
		return;
	}

	CEE_Device *dev = (CEE_Device *) t->user_data;
	dev->out_completion(t);
}

void CEE_Device::out_completion(libusb_transfer* t) {
	std::lock_guard<std::mutex> lock(m_state);
	m_out_transfers.num_active--;

	if (t->status == LIBUSB_TRANSFER_COMPLETED) {
		if (m_session->m_cancellation == 0) {
			submit_out_transfer(t);
		}
	} else if (t->status != LIBUSB_TRANSFER_CANCELLED) {
		m_session->handle_error(t->status, "CEE_Device::out_completion");
	}

	if (m_out_transfers.num_active == 0 && m_in_transfers.num_active == 0) {
		m_session->completion();
	}
}

void CEE_Device::configure(uint64_t rate) {
	double sampleTime = 1.0/rate;
	m_xmega_per = round(sampleTime * (double) CEE_timer_clock);
	if (m_xmega_per < m_min_per) m_xmega_per = m_min_per;
	sampleTime = m_xmega_per / (double) CEE_timer_clock; // convert back to get the actual sample time;

	unsigned transfers = 4;
	m_packets_per_transfer = ceil(BUFFER_TIME / (sampleTime * 10) / transfers);

	m_in_transfers.alloc( transfers, m_usb, EP_IN,  LIBUSB_TRANSFER_TYPE_BULK, m_packets_per_transfer*sizeof(IN_packet),  1000, cee_in_completion,  this);
	m_out_transfers.alloc(transfers, m_usb, EP_OUT, LIBUSB_TRANSFER_TYPE_BULK, m_packets_per_transfer*sizeof(OUT_packet), 1000, cee_out_completion, this);

	m_in_transfers.num_active = m_out_transfers.num_active = 0;

	smu_debug("CEE prepare %i transfers %u\n", m_xmega_per, m_packets_per_transfer);
}

inline uint16_t CEE_Device::encode_out(unsigned chan, uint32_t igain) {
	int v = 0;
	if (m_mode[chan] == SVMI) {
		float val = m_signals[chan][0].get_sample();
		val = constrain(val, 0, 5.0);
		v = 4095*val/5.0;
	} else if (m_mode[chan] == SIMV) {
		float val = m_signals[chan][1].get_sample();
		val = constrain(val, m_current_limit / -1000.0, m_current_limit / 1000.0);
		v = 4095*(1.25+(igain/CEE_current_gain_scale)*val)/2.5;
	}
	if (v > 4095) v = 4095;
	if (v < 0) v = 0;
	return v;
}

bool CEE_Device::submit_out_transfer(libusb_transfer* t) {
	if (m_sample_count == 0 || m_out_sampleno < m_sample_count) {
		for (unsigned p=0; p<m_packets_per_transfer; p++) {
			OUT_packet *pkt = &((OUT_packet *)t->buffer)[p];
			pkt->mode_a = m_mode[0];
			pkt->mode_b = m_mode[1];

			for (unsigned i=0; i<OUT_SAMPLES_PER_PACKET; i++) {
				pkt->data[i].pack(
					encode_out(0, m_cal.current_gain_a),
					encode_out(1, m_cal.current_gain_b)
				);
				m_out_sampleno++;
			}
		}

		int r = libusb_submit_transfer(t);
		if (r != 0) {
			smu_debug("libusb_submit_transfer out: %i\n", r);
		}
		m_out_transfers.num_active++;
		return true;
	}
	return false;
}

bool CEE_Device::submit_in_transfer(libusb_transfer* t) {
	if (m_sample_count == 0 || m_requested_sampleno < m_sample_count) {
		int r = libusb_submit_transfer(t);
		if (r != 0) {
			smu_debug("libusb_submit_transfer in: %i\n", r);
		}
		m_in_transfers.num_active++;
		m_requested_sampleno += m_packets_per_transfer*IN_SAMPLES_PER_PACKET;
		return true;
	}
	return false;
}

void CEE_Device::handle_in_transfer(libusb_transfer* t) {
	bool rawMode = 0;
	float v_factor = 5.0/2048.0;
	float i_factor_a = 2.5/2048.0/(m_cal.current_gain_a/CEE_current_gain_scale) / 2;
	float i_factor_b = 2.5/2048.0/(m_cal.current_gain_b/CEE_current_gain_scale) / 2;
	if (rawMode) v_factor = i_factor_a = i_factor_b = 1;

	for (unsigned p=0; p<m_packets_per_transfer; p++) {
		IN_packet *pkt = &((IN_packet*)t->buffer)[p];

		if ((pkt->flags & FLAG_PACKET_DROPPED) && m_in_sampleno != 0) {
			smu_debug("Warning: dropped packet\n");
		}

		for (int i=0; i<IN_SAMPLES_PER_PACKET; i++) {
			m_signals[0][0].put_sample((m_cal.offset_a_v + pkt->data[i].av())*v_factor);
			if (m_mode[0] != DISABLED) {
				m_signals[0][1].put_sample((m_cal.offset_a_i + pkt->data[i].ai())*i_factor_a);
			} else {
				m_signals[0][1].put_sample(0);
			}

			m_signals[1][0].put_sample((m_cal.offset_b_v + pkt->data[i].bv())*v_factor);
			if (m_mode[1] != DISABLED) {
				m_signals[1][1].put_sample((m_cal.offset_b_i + pkt->data[i].bi())*i_factor_b);
			} else {
				m_signals[1][1].put_sample(0);
			}
			m_in_sampleno++;
		}
	}

	m_session->progress();
}

const sl_device_info* CEE_Device::info() const {
	return &cee_info;
}

const sl_channel_info* CEE_Device::channel_info(unsigned channel) const {
	if (channel < 2) {
		return &cee_channel_info[channel];
	} else {
		return NULL;
	}
}

Signal* CEE_Device::signal(unsigned channel, unsigned signal) {
	if (channel < 2 && signal < 2) {
		return &m_signals[channel][signal];
	} else {
		return NULL;
	}
}

void CEE_Device::set_mode(unsigned chan, unsigned mode) {
	if (chan < 2) {
		m_mode[chan] = mode;
	}
}

void CEE_Device::on() {
	libusb_control_transfer(m_usb, 0x40, CMD_CONFIG_CAPTURE, m_xmega_per, DEVMODE_2SMU, 0, 0, 100);
}

void CEE_Device::start_run(uint64_t samples) {
	std::lock_guard<std::mutex> lock(m_state);
	m_sample_count = samples;
	m_requested_sampleno = m_in_sampleno = m_out_sampleno = 0;

	for (auto i: m_in_transfers) {
		if (!submit_in_transfer(i)) break;
	}

	for (auto i: m_out_transfers) {
		if (!submit_out_transfer(i)) break;
	}
}

void CEE_Device::cancel() {
	m_in_transfers.cancel();
	m_out_transfers.cancel();
}

void CEE_Device::off() {
	libusb_control_transfer(m_usb, 0x40, CMD_CONFIG_CAPTURE, 0, DEVMODE_OFF, 0, 0, 100);
}
