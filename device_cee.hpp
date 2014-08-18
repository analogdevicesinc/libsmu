#pragma once
#include <mutex>
#include <condition_variable>
#include "libsmu.hpp"
#include "internal.hpp"

struct libusb_device_handle;

class CEE_Device: public Device {
public:
	virtual ~CEE_Device();
	virtual sl_device_info* info();
	virtual sl_channel_info* channel_info(unsigned channel);
	//virtual sl_mode_info* mode_info(unsigned mode);
	virtual Signal* signal(unsigned channel, unsigned signal);
	virtual void set_mode(unsigned mode);

protected:
	friend class Session;
	CEE_Device(Session* s, libusb_device* device);
	virtual int init();
	virtual int added();
	virtual int removed();
	virtual void configure(uint64_t sampleRate);
	virtual void start(sample_t nsamples);
	virtual void update();
	virtual void cancel();

	std::string m_hw_version;
	std::string m_fw_version;
	std::string m_git_version;

	Transfers m_in_transfers;
	Transfers m_out_transfers;

	std::mutex m_state;
	std::condition_variable m_completion;

	struct EEPROM_cal{
		uint32_t magic;
		int8_t offset_a_v, offset_a_i, offset_b_v, offset_b_i;
		int16_t dac200_a, dac200_b, dac400_a, dac400_b;
		uint32_t current_gain_a, current_gain_b;
		uint8_t flags; // bit 0: USB powered
	} __attribute__((packed));

	void read_calibration();
	EEPROM_cal m_cal;

	void set_current_limit(unsigned mode);
	unsigned m_current_limit;
	int m_min_per;
	int m_xmega_per;

	unsigned m_mode;
	uint64_t m_sample_rate;
	uint64_t m_sample_count;

	// State owned by USB thread
	uint16_t m_requested_sampleno;
	uint64_t m_in_sampleno;
	uint64_t m_out_sampleno;
};
