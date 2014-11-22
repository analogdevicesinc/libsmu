#include "device_m1000.hpp"
#include <libusb-1.0/libusb.h>
#include <iostream>
#include <cstring>
#include <cmath>
#include <cassert>

using std::cout;
using std::cerr;
using std::endl;

#define EP_OUT 0x02
#define EP_IN 0x81

const unsigned chunk_size = 256;
#define OUT_SAMPLES_PER_PACKET chunk_size
#define IN_SAMPLES_PER_PACKET chunk_size
#define A 0
#define B 1

const unsigned out_packet_size = chunk_size * 2 * 2;
const unsigned in_packet_size = chunk_size * 4 * 2;

const int M1K_timer_clock = 3e6; // 96MHz/32 = 3MHz
const int m_min_per = 0x1F;
int m_sam_per = 0;
extern "C" void LIBUSB_CALL m1000_in_transfer_callback(libusb_transfer *t);
extern "C" void LIBUSB_CALL m1000_out_transfer_callback(libusb_transfer *t);

const double m1000_default_sample_time = 1/48000.0;

#ifdef _WIN32
const double BUFFER_TIME = 0.050;
#else
const double BUFFER_TIME = 0.020;
#endif

static const sl_device_info m1000_info = {DEVICE_M1000,"M1000", 2};

static const sl_channel_info m1000_channel_info[2] = {
	{CHANNEL_SMU, "A", 3, 2},
	{CHANNEL_SMU, "B", 3, 2},
};

// Mode 0: high-z
// Mode 1: SVMI
// Mode 2: SIMV

enum M1000_chanmode{
	DISABLED = 0,
	SVMI = 1,
	SIMV = 2,
};

static const sl_signal_info m1000_signal_info[2] = {
	{ SIGNAL, "Voltage", 0x7, 0x2, unit_V,  0.0, 5.0, 5.0/65536 },
	{ SIGNAL, "Current", 0x6, 0x4, unit_A, -0.2, 0.2, 0.4/65536 },
};

const float current_limit = 0.2;

M1000_Device::M1000_Device(Session* s, libusb_device* device):
	Device(s, device),
	m_signals{
		{Signal(&m1000_signal_info[0]), Signal(&m1000_signal_info[1])},
		{Signal(&m1000_signal_info[0]), Signal(&m1000_signal_info[1])},
	},
	m_mode{0,0}
{	}

M1000_Device::~M1000_Device() {}

int M1000_Device::init() {
	int r = Device::init();
	if (r!=0) { return r; }
	return 0;
}

int M1000_Device::added() {
	libusb_claim_interface(m_usb, 0);
	return 0;
}

int M1000_Device::removed() {
	set_mode(0, 0);
	set_mode(1, 0);
	libusb_release_interface(m_usb, 0);
	return 0;
}


/// Runs in USB thread
extern "C" void LIBUSB_CALL m1000_in_completion(libusb_transfer *t){
	M1000_Device *dev = (M1000_Device *) t->user_data;
	std::lock_guard<std::mutex> lock(dev->m_state);

	if (t->status == LIBUSB_TRANSFER_COMPLETED){
		dev->handle_in_transfer(t);
		dev->submit_in_transfer(t);
	}else{
		std::cerr << "ITransfer error "<< libusb_error_name(t->status) << " " << t << std::endl;
		//TODO: notify main thread of error
	}
}

/// Runs in USB thread
extern "C" void LIBUSB_CALL m1000_out_completion(libusb_transfer *t){
	M1000_Device *dev = (M1000_Device *) t->user_data;
	std::lock_guard<std::mutex> lock(dev->m_state);

	if (t->status == LIBUSB_TRANSFER_COMPLETED){
		dev->submit_out_transfer(t);
	}else{
		std::cerr << "OTransfer error "<< libusb_error_name(t->status) << " " << t << std::endl;
	}
}

void M1000_Device::configure(uint64_t rate) {
	double sample_time = 1.0/rate;
	m_sam_per = round(sample_time * (double) M1K_timer_clock);
	if (m_sam_per < m_min_per) m_sam_per = m_min_per;
	sample_time = m_sam_per / (double) M1K_timer_clock; // convert back to get the actual sample time;

	unsigned transfers = 8;
	m_packets_per_transfer = ceil(BUFFER_TIME / (sample_time * chunk_size) / transfers);

	m_in_transfers.alloc( transfers, m_usb, EP_IN,  LIBUSB_TRANSFER_TYPE_BULK,
		m_packets_per_transfer*in_packet_size,  10000, m1000_in_completion,  this);
	m_out_transfers.alloc(transfers, m_usb, EP_OUT, LIBUSB_TRANSFER_TYPE_BULK,
		m_packets_per_transfer*out_packet_size, 10000, m1000_out_completion, this);

	//std::cerr << "M1000 prepare " << transfers <<  " " << m_packets_per_transfer << std::endl;
	//std::cerr << "M1000 rate " << sample_time <<  " " << m_sam_per << std::endl;
}

inline uint16_t M1000_Device::encode_out(int chan) {
	int v = 0;
	if (m_mode[chan] == SVMI) {
		float val = m_signals[chan][0].get_sample();
		val = constrain(val, 0, 5.0);
		v = 65535*val/5.0;
	} else if (m_mode[chan] == SIMV) {
		float val = m_signals[chan][1].get_sample();
		val = constrain(val, -current_limit, current_limit);
		v = 65536*(2.5 * 4./5. + 5.*.2*20.*0.5*val)/5.0;
	} else if (m_mode[chan] == DISABLED) {
		v = 24000;
	}
	if (v > 65535) v = 65535;
	if (v < 0) v = 0;
	return v;
}

bool M1000_Device::submit_out_transfer(libusb_transfer* t) {
	if (m_sample_count == 0 || m_out_sampleno < m_sample_count) {
		//std::cerr << "submit_out_transfer " << m_out_sampleno << std::endl;

		for (int p=0; p<m_packets_per_transfer; p++){
			uint16_t* buf = (uint16_t*) (t->buffer + p*out_packet_size);
			for (int i=0; i < chunk_size; i++){
				buf[i+chunk_size*0] = htobe16(encode_out(0));
				buf[i+chunk_size*1] = htobe16(encode_out(1));
				m_out_sampleno++;
			}
		}

		int r = libusb_submit_transfer(t);
		if (r != 0) {
			//cerr << "libusb_submit_transfer out " << r << endl;
		}
		return true;
	}/* else {
		std::cerr << "out done" << std::endl;
	}*/
	return false;
}

bool M1000_Device::submit_in_transfer(libusb_transfer* t) {
	if (m_sample_count == 0 || m_requested_sampleno < m_sample_count) {
		//std::cerr << "submit_in_transfer " << m_requested_sampleno << std::endl;
		int r = libusb_submit_transfer(t);
		if (r != 0) {
		//	cerr << "libusb_submit_transfer in " << r << endl;
		}
		m_requested_sampleno += m_packets_per_transfer*IN_SAMPLES_PER_PACKET;
		return true;
	}
	//std::cerr << "not resubmitting" << endl;
	return false;
}

void M1000_Device::handle_in_transfer(libusb_transfer* t) {

	for (int p=0; p<m_packets_per_transfer; p++){
		uint16_t* buf = (uint16_t*) (t->buffer + p*in_packet_size);

		for (int i=(m_in_sampleno==0)?2:0; i<chunk_size; i++){
			m_signals[0][0].put_sample( be16toh(buf[i+chunk_size*0]) / 65535.0 * 5.0);
			m_signals[0][1].put_sample((be16toh(buf[i+chunk_size*1]) / 65535.0 - 0.61) * 0.4 + 0.048);
			m_signals[1][0].put_sample( be16toh(buf[i+chunk_size*2]) / 65535.0 * 5.0);
			m_signals[1][1].put_sample((be16toh(buf[i+chunk_size*3]) / 65535.0 - 0.61) * 0.4 + 0.048);
			m_in_sampleno++;
		}
	}

	m_session->progress();
	if (m_in_sampleno >= m_sample_count) {
		assert(m_out_sampleno >= m_sample_count);
		m_session->completion();
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

void M1000_Device::set_mode(unsigned chan, unsigned mode)
{
	if (chan < 2) {
		m_mode[chan] = mode;
	}
	libusb_control_transfer(m_usb, 0x40, 0x53, chan, mode, 0, 0, 100);
}

void M1000_Device::on()
{
	libusb_set_interface_alt_setting(m_usb, 0, 1);

	libusb_control_transfer(m_usb, 0x40, 0xCC, 0, 0, 0, 0, 100);
}

void M1000_Device::start_run(uint64_t samples) {
	std::lock_guard<std::mutex> lock(m_state);
	m_sample_count = samples;
	m_requested_sampleno = m_in_sampleno = m_out_sampleno = 0;
	libusb_control_transfer(m_usb, 0x40, 0xC5, 1, m_sam_per, 0, 0, 100);

	for (auto i: m_in_transfers) {
		if (!submit_in_transfer(i)) break;
	}

	for (auto i: m_out_transfers) {
		if (!submit_out_transfer(i)) break;
	}
}

void M1000_Device::cancel()
{
}

void M1000_Device::off()
{
	libusb_control_transfer(m_usb, 0x40, 0xC5, 0x0000, 0x0000, 0, 0, 100);
	// disable AFE is slow, requires ~10ms to come to steady state
	// don't turn it off
	//libusb_control_transfer(m_usb, 0x40, 0x50, 49, 0, 0, 0, 100);
}
