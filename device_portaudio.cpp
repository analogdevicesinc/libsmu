#include "device_portaudio.hpp"
#include <portaudio.h>
#include <iostream>
#include <cstring>
#include <cmath>
#include <cassert>
#include <mutex>

using std::cerr;
using std::endl;

const sl_signal_info portaudio_signal_info[2] =  {
	{ SIGNAL, "Count", MODE_HIGH_Z, MODE_HIGH_Z, unit_none, 0, 1, 1.0/65536 }, // Input
	{ SIGNAL, "Count", MODE_HIGH_Z, MODE_HIGH_Z, unit_none, 0, 1, 1.0/65536 }, // Output
};

static const sl_device_info pa_info = {DEVICE_PORTAUDIO, "PortAudio", 1};

extern "C" int PaStreamCallback_fn( const void *input,
                                      void *output,
                                      unsigned long frameCount,
                                      const PaStreamCallbackTimeInfo* timeInfo,
                                      PaStreamCallbackFlags statusFlags,
                                      void *userData );

PortAudio_Device::PortAudio_Device(Session* s, libusb_device* d): 
	Device(s, d),
	m_signals {
		Signal(&portaudio_signal_info[0]),
		Signal(&portaudio_signal_info[0]),
		Signal(&portaudio_signal_info[1]),
	},
{}

PortAudio_Device::~PortAudio_Device() {}

int PortAudio_Device::get_default_rate() {
	return 48000;
}

int PortAudio_Device::init() {
	return Pa_Initialize();
}

void PortAudio_Device::configure(uint64_t sample_rate) {
    Pa_OpenDefaultStream( &m_stream,
                                1,          /* one input channels */
                                2,          /* stereo output */
                                paFloat32,  /* 32 bit floating point output */
                                sample_rate,
                                256,        /* frames per buffer, i.e. the number
                                                   of sample frames that PortAudio will
                                                   request from the callback. Many apps
                                                   may want to use
                                                   paFramesPerBufferUnspecified, which
                                                   tells PortAudio to pick the best,
                                                   possibly changing, buffer size.*/
                                PaStreamCallback_fn, /* this is your callback function */
                                this); /*This is a pointer that will be passed to
                                                   your callback*/
}

void PortAudio_Device::start_run(uint64_t samples) {
	Pa_StartStream( m_stream );
}

void PortAudio_Device::cancel() {
	Pa_CloseStream( m_stream );
}

void PortAudio_Device::off() {
	Pa_Terminate();
}

extern "C" int PaStreamCallback_fn( const void *input,
                                      void *output,
                                      unsigned long frameCount,
                                      const PaStreamCallbackTimeInfo* timeInfo,
                                      PaStreamCallbackFlags statusFlags,
                                      void *userData ) {

	PortAudio_Device *dev = (PortAudio_Device *) (userData);
	std::lock_guard<std::mutex> lock(dev->m_state);

    float *out = (float*)output;
	float *in = (float*)input;
    
    for( int i=0; i<frameCount; i++ )
    {
        *out++ = dev->m_signals[0].get_sample();
        *out++ = dev->m_signals[0].get_sample();
		dev->m_out_sampleno++;
		dev->m_signals[2].put_sample(in[i]);
		dev->m_in_sampleno++;
    }
	
	dev->m_session->progress();
    return 0;
}
