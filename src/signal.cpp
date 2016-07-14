// Released under the terms of the BSD License
// (C) 2014-2016
//   Analog Devices, Inc.
//   Kevin Mehall <km@kevinmehall.net>
//   Ian Daniher <itdaniher@gmail.com>

#ifdef WIN32
#define _USE_MATH_DEFINES // needed for VS to define math constants (e.g. M_PI)
#endif

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>

#include <libsmu/libsmu.hpp>

using namespace smu;

Signal::~Signal()
{
	if(m_src_buf)
		delete[] m_src_buf;
}

void Signal::source_constant(float val)
{
	m_src = SRC_CONSTANT;
	m_src_v1 = val;
}

void Signal::source_square(float midpoint, float peak, double period, double duty, double phase)
{
	m_src = SRC_SQUARE;
	m_src_v1 = midpoint;
	m_src_v2 = peak;
	m_src_period = period;
	m_src_duty = duty;
	m_src_phase = phase;
}

void Signal::source_sawtooth(float midpoint, float peak, double period, double phase)
{
	m_src = SRC_SAWTOOTH;
	m_src_v1 = midpoint;
	m_src_v2 = peak;
	m_src_period = period;
	m_src_phase = phase;
}

void Signal::source_stairstep(float midpoint, float peak, double period, double phase)
{
	m_src = SRC_STAIRSTEP;
	m_src_v1 = midpoint;
	m_src_v2 = peak;
	m_src_period = period;
	m_src_phase = phase;
}

void Signal::source_sine(float midpoint, float peak, double period, double phase)
{
	m_src = SRC_SINE;
	m_src_v1 = midpoint;
	m_src_v2 = peak;
	m_src_period = period;
	m_src_phase = phase;
}

void Signal::source_triangle(float midpoint, float peak, double period, double phase)
{
	m_src = SRC_TRIANGLE;
	m_src_v1 = midpoint;
	m_src_v2 = peak;
	m_src_period = period;
	m_src_phase = phase;
}

void Signal::source_buffer(float* buf, size_t len, bool repeat)
{
	m_src = SRC_BUFFER;
	m_src_buf = buf;
	m_src_buf_len = len;
	m_src_buf_repeat = repeat;
	m_src_i = 0;
}

void Signal::source_callback(std::function<float (uint64_t index)> callback)
{
	m_src = SRC_CALLBACK;
	m_src_callback = callback;
	m_src_i = 0;
}

float Signal::get_sample()
{
	switch (m_src) {
	case SRC_CONSTANT:
		return m_src_v1;

	case SRC_BUFFER:
		if (m_src_i >= m_src_buf_len) {
			if (m_src_buf_repeat) {
				m_src_i = 0;
			} else {
				return m_src_buf[m_src_buf_len - 1];
			}
		}
		return m_src_buf[m_src_i++];


	case SRC_CALLBACK:
		return m_src_callback(m_src_i++);

	case SRC_SQUARE:
	case SRC_SAWTOOTH:
	case SRC_SINE:
	case SRC_STAIRSTEP:
	case SRC_TRIANGLE:

		auto peak_to_peak = m_src_v2 - m_src_v1;
		auto phase = m_src_phase;
		auto norm_phase = phase / m_src_period;
		if (norm_phase < 0)
			norm_phase++;
		m_src_phase = fmod(m_src_phase + 1, m_src_period);

		switch (m_src) {
		case SRC_SQUARE:
			return (norm_phase < m_src_duty) ? m_src_v1 : m_src_v2;

		case SRC_SAWTOOTH: {
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

		case SRC_STAIRSTEP:
			return m_src_v2 - floorf(norm_phase * 10) * peak_to_peak / 9;

		case SRC_SINE:
			return m_src_v1 + (1 + cos(norm_phase * 2 * M_PI)) * peak_to_peak / 2;

		case SRC_TRIANGLE:
			return m_src_v1 + fabs(1 - norm_phase * 2) * peak_to_peak;
		default:
			return 0;
		}
	}
	return 0;
}
