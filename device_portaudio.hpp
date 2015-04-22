#pragma once

#include "libsmu.hpp"
#include <portaudio.h>
#include <libusb.h>
#include <mutex>

extern "C" int PaStreamCallback_fn( const void *input,
	void *output,
	unsigned long frameCount,
	const PaStreamCallbackTimeInfo* timeInfo,
	PaStreamCallbackFlags statusFlags,
	void *userData );

class PortAudio_Device: public Device {
public:
	virtual ~PortAudio_Device();

protected:
	friend class Session;
	friend int PaStreamCallback_fn( const void *input,
                                      void *output,
                                      unsigned long frameCount,
                                      const PaStreamCallbackTimeInfo* timeInfo,
                                      PaStreamCallbackFlags statusFlags,
                                      void *userData );

	PortAudio_Device(Session *m_session, libusb_device* device);
    PaStream* stream;
	virtual int init();
	virtual int get_default_rate();
	virtual void configure(uint64_t sampleRate);
	virtual void start_run(sample_t nsamples);
	virtual void cancel();
	virtual void on();
	virtual void off();
	virtual const char* serial() const { return "0"; };
	int m_sample_count = 0;
	sample_t m_inout_sampleno = 0;
	Signal m_signals[3];
	PaStream *m_stream;
};
