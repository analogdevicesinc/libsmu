#include "device_cee.hpp"
#include <libusb-1.0/libusb.h>
#include <iostream>
#include <cstring>
#include <cmath>

using std::cout;
using std::cerr;
using std::endl;

#define EP_BULK_IN 0x81
#define EP_BULK_OUT 0x02

#define CMD_CONFIG_CAPTURE 0x80
#define CMD_CONFIG_GAIN 0x65
#define CMD_ISET_DAC 0x15
#define DEVMODE_OFF  0
#define DEVMODE_2SMU 1

extern "C" void LIBUSB_CALL in_completion(libusb_transfer *t);
extern "C" void LIBUSB_CALL out_completion(libusb_transfer *t);

const int CEE_timer_clock = 4e6; // 4 MHz
const double CEE_default_sample_time = 1/10000.0;
const double CEE_current_gain_scale = 100000;
const uint32_t CEE_default_current_gain = 45*.07*CEE_current_gain_scale;

const int default_current_limit = 200;

#ifdef _WIN32
const double BUFFER_TIME = 0.050;
#else
const double BUFFER_TIME = 0.020;
#endif

const sl_channel_info channel_info[2] = {
	{CHANNEL_SMU, "A", 3, 2},
	{CHANNEL_SMU, "B", 3, 2},
};

// Mode 0: high-z
// Mode 1: SVMI
// Mode 2: SIMV

const sl_signal_info signal_info[2] = {
	{ SIGNAL, "Voltage", 0x7, 0x2, unit_V,  0.0, 5.0, 5.0/4095 },
	{ SIGNAL, "Current", 0x6, 0x4, unit_A, -0.2, 0.2, 400.0/4095 },
};

enum CEE_chanmode{
	DISABLED = 0,
	SVMI = 1,
	SIMV = 2,
};

inline int16_t signextend12(uint16_t v){
	return (v>((1<<11)-1))?(v - (1<<12)):v;
}

struct IN_sample{
	uint8_t avl, ail, aih_avh, bvl, bil, bih_bvh;

	int16_t av(){return signextend12((aih_avh&0x0f)<<8) | avl;}
	int16_t bv(){return signextend12((bih_bvh&0x0f)<<8) | bvl;}
	int16_t ai(){return signextend12((aih_avh&0xf0)<<4) | ail;}
	int16_t bi(){return signextend12((bih_bvh&0xf0)<<4) | bil;}
} __attribute__((packed));

#define IN_SAMPLES_PER_PACKET 10
#define FLAG_PACKET_DROPPED (1<<0)

typedef struct IN_packet{
	uint8_t mode_a;
	uint8_t mode_b;
	uint8_t flags;
	uint8_t mode_seq;
	IN_sample data[10];
} __attribute__((packed)) IN_packet;

#define OUT_SAMPLES_PER_PACKET 10
struct OUT_sample{
	uint8_t al, bl, bh_ah;
	void pack(uint16_t a, uint16_t b){
		al = a & 0xff;
		bl = b & 0xff;
		bh_ah = ((b>>4)&0xf0) |	(a>>8);
	}
} __attribute__((packed));

typedef struct OUT_packet{
	uint8_t mode_a;
	uint8_t mode_b;
	OUT_sample data[10];
} __attribute__((packed)) OUT_packet;

#define EEPROM_VALID_MAGIC 0x90e26cee
#define EEPROM_FLAG_USB_POWER (1<<0)

typedef struct CEE_version_descriptor{
	uint8_t version_major;
	uint8_t version_minor;
	uint8_t flags;
	uint8_t per_ns;
	uint8_t min_per;
} __attribute__((packed)) CEE_version_descriptor;

CEE_Device::CEE_Device(Session* s, libusb_device* device): Device(s, device) {}
CEE_Device::~CEE_Device() {}

int CEE_Device::init() {
	int r = Device::init();
	if (r!=0) { return r; }

	char buf[64];
	r = libusb_control_transfer(m_usb, 0xC0, 0x00, 0, 0, (uint8_t*)buf, 64, 100);
	if (r >= 0){
		m_hw_version = std::string(buf, strnlen(buf, r));
	}

	r = libusb_control_transfer(m_usb, 0xC0, 0x00, 0, 1, (uint8_t*)buf, 64, 100);
	if (r >= 0){
		m_fw_version = std::string(buf, strnlen(buf, r));
	}

	CEE_version_descriptor version_info;
	bool have_version_info = 0;

	if (m_fw_version >= "1.2"){
		r = libusb_control_transfer(m_usb, 0xC0, 0x00, 0, 0xff, (uint8_t*)&version_info, sizeof(version_info), 100);
		have_version_info = (r>=0);

		r = libusb_control_transfer(m_usb, 0xC0, 0x00, 0, 2, (uint8_t*)buf, 64, 100);
		if (r >= 0){
			m_git_version = std::string(buf, strnlen(buf, r));
		}
	}

	if (have_version_info){
		m_min_per = version_info.min_per;
		if (version_info.per_ns != 250){
			std::cerr << "    Error: alternate timer clock " << version_info.per_ns << " is not supported in this release." << std::endl;
		}

	}else{
		m_min_per = 100;
	}

	double minSampleTime = m_min_per / (double) CEE_timer_clock;

	std::cerr << "    Hardware: " << m_hw_version << std::endl;
	std::cerr << "    Firmware version: " << m_fw_version << " (" << m_git_version << ")" << std::endl;
	std::cerr << "    Supported sample rate: " << CEE_timer_clock / m_min_per / 1000.0 << "ksps" << std::endl;
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

void CEE_Device::read_calibration(){
	union{
		uint8_t buf[64];
		uint32_t magic;
	};
	int r = libusb_control_transfer(m_usb, 0xC0, 0xE0, 0, 0, buf, 64, 100);
	if (!r || magic != EEPROM_VALID_MAGIC){
		cerr << "    Reading calibration data failed " << r << endl;
		memset(&m_cal, 0xff, sizeof(m_cal));

		m_cal.offset_a_v = m_cal.offset_a_i = m_cal.offset_b_v = m_cal.offset_b_i = 0;
		m_cal.dac400_a = m_cal.dac400_b = m_cal.dac200_a = m_cal.dac200_b = 0x6B7;
	}else{
		memcpy((uint8_t*)&m_cal, buf, sizeof(m_cal));
	}

	set_current_limit((m_cal.flags&EEPROM_FLAG_USB_POWER)?default_current_limit:2000);

	if (m_cal.current_gain_a == (uint32_t) -1){
		m_cal.current_gain_a = CEE_default_current_gain;
	}

	if (m_cal.current_gain_b == (uint32_t) -1){
		m_cal.current_gain_b = CEE_default_current_gain;
	}

	cerr << "    Current gain " << m_cal.current_gain_a << " " << m_cal.current_gain_b << endl;
}

void CEE_Device::set_current_limit(unsigned mode) {
	unsigned ilimit_cal_a, ilimit_cal_b;

	if (mode == 200){
		ilimit_cal_a = m_cal.dac200_a;
		ilimit_cal_b = m_cal.dac200_b;
	}else if(mode == 400){
		ilimit_cal_a = m_cal.dac400_a;
		ilimit_cal_b = m_cal.dac400_b;
	}else if (mode == 2000){
		ilimit_cal_a = ilimit_cal_b = 0;
	}else{
		std::cerr << "Invalid current limit " << mode << std::endl;
		return;
	}

	libusb_control_transfer(m_usb, 0xC0, CMD_ISET_DAC, ilimit_cal_a, ilimit_cal_b, NULL, 0, 100);
	m_current_limit = mode;
}

void CEE_Device::configure(uint64_t rate) {
	double sampleTime = 1/rate;
	m_xmega_per = round(sampleTime * (double) CEE_timer_clock);
	if (m_xmega_per < m_min_per) m_xmega_per = m_min_per;
	sampleTime = m_xmega_per / (double) CEE_timer_clock; // convert back to get the actual sample time;

	unsigned transfers = 4;
	unsigned packets_per_transfer = ceil(BUFFER_TIME / (sampleTime * 10) / transfers);

	std::cerr << "CEE prepare "<< m_xmega_per << " " << transfers <<  " " << packets_per_transfer  << m_current_limit << std::endl;
}

sl_device_info* CEE_Device::info()
{
	return NULL;
}

sl_channel_info* CEE_Device::channel_info(unsigned channel)
{
	return NULL;
}

Signal* CEE_Device::signal(unsigned channel, unsigned signal)
{
	return NULL;
}

void CEE_Device::set_mode(unsigned mode)
{

}

void CEE_Device::start(uint64_t samples)
{

}

void CEE_Device::update()
{

}

void CEE_Device::cancel()
{

}
