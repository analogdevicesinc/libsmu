// Released under the terms of the BSD License
// (C) 2014-2017
//   Analog Devices, Inc
//   Kevin Mehall <km@kevinmehall.net>
//   Ian Daniher <itdaniher@gmail.com>

#ifdef WIN32
#define _USE_MATH_DEFINES // needed for VS to define math constants (e.g. M_PI)
#endif

#include <cmath>
#include <vector>

#include "debug.hpp"
#include <libsmu/libsmu.hpp>

using namespace smu;

void Signal::constant(std::vector<float>& buf, uint64_t samples, float val)
{
	m_src = CONSTANT;
	m_src_v1 = val;

	for (unsigned i = 0; i < samples; i++) {
		buf.push_back(get_sample());
	}
}

void Signal::square(std::vector<float>& buf, uint64_t samples, float midpoint, float peak, double period, double phase, double duty)
{
	m_src = SQUARE;
	m_src_phase = phase;
	m_src_period = period;
	m_src_v1 = midpoint;
	m_src_v2 = peak;
	m_src_duty = duty;

	for (unsigned i = 0; i < samples; i++) {
		buf.push_back(get_sample());
	}
}

void Signal::sawtooth(std::vector<float>& buf, uint64_t samples, float midpoint, float peak, double period, double phase)
{
	m_src = SAWTOOTH;
	m_src_phase = phase;
	m_src_period = period;
	m_src_v1 = midpoint;
	m_src_v2 = peak;

	for (unsigned i = 0; i < samples; i++) {
		buf.push_back(get_sample());
	}
}

void Signal::stairstep(std::vector<float>& buf, uint64_t samples, float midpoint, float peak, double period, double phase)
{
	m_src = STAIRSTEP;
	m_src_phase = phase;
	m_src_period = period;
	m_src_v1 = midpoint;
	m_src_v2 = peak;

	for (unsigned i = 0; i < samples; i++) {
		buf.push_back(get_sample());
	}
}

void Signal::sine(std::vector<float>& buf, uint64_t samples, float midpoint, float peak, double period, double phase)
{
	m_src = SINE;
	m_src_phase = phase;
	m_src_period = period;
	m_src_v1 = midpoint;
	m_src_v2 = peak;

	for (unsigned i = 0; i < samples; i++) {
		buf.push_back(get_sample());
	}
}

void Signal::triangle(std::vector<float>& buf, uint64_t samples, float midpoint, float peak, double period, double phase)
{
	m_src = TRIANGLE;
	m_src_phase = phase;
	m_src_period = period;
	m_src_v1 = midpoint;
	m_src_v2 = peak;

	for (unsigned i = 0; i < samples; i++) {
		buf.push_back(get_sample());
	}
}

// Internal function to generate waveform values.
float Signal::get_sample()
{
	switch (m_src) {
		case CONSTANT:
			return m_src_v1;

		case SQUARE:
		case SAWTOOTH:
		case SINE:
		case STAIRSTEP:
		case TRIANGLE: {

			auto peak_to_peak = m_src_v2 - m_src_v1;
			auto phase = m_src_phase;
			auto norm_phase = phase / m_src_period;
			if (norm_phase < 0)
				norm_phase += 1;
			m_src_phase = fmod(m_src_phase + 1, m_src_period);

			switch (m_src) {
				case SQUARE:
					return (norm_phase < m_src_duty) ? m_src_v1 : m_src_v2;

				case SAWTOOTH: {
					float int_period = truncf(m_src_period);
					float int_phase = truncf(phase);
					float frac_period = m_src_period - int_period;
					float frac_phase = phase - int_phase;
					float max_int_phase;

					// Get the integer part of the maximum value phase will be set at.
					// For example:
					// - If m_src_period = 100.6, phase first value = 0.3 then
					//   phase will take values: 0.3, 1.3, ..., 98.3, 99.3, 100.3
					// - If m_src_period = 100.6, phase first value = 0.7 then
					//   phase will take values: 0.7, 1.7, ..., 98.7, 99.7
					if (frac_period <= frac_phase)
						max_int_phase = int_period - 1;
					else
						max_int_phase = int_period;

					return m_src_v2 - int_phase / max_int_phase * peak_to_peak;
				}

				case STAIRSTEP:
					return m_src_v2 - floorf(norm_phase * 10) * peak_to_peak / 9;

				case SINE:
					return m_src_v1 + (1 + cos(norm_phase * 2 * M_PI)) * peak_to_peak / 2;

				case TRIANGLE:
					return m_src_v1 + fabs(1 - norm_phase * 2) * peak_to_peak;
				default:
					throw std::runtime_error("unknown waveform");
			}
		}
	}
	throw std::runtime_error("unknown waveform");
}
