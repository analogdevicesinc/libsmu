// Released under the terms of the BSD License
// (C) 2016, Analog Devices, Inc.

/// @file signal.hpp
/// @brief Signal handling.

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

#include <libusb.h>

namespace smu {
	class Device;
	class Signal;
}

typedef enum sl_type {
	DEVICE_M1000 = 0x10000,
	CHANNEL_SMU = 0x20000,
	MODE_HIGH_Z = 0x40000,
	MODE_SVMI,
	MODE_SIMV,
	SIGNAL = 0x80000,
} sl_type;

typedef struct sl_unit {
	int8_t m;
	int8_t kg;
	int8_t s;
	int8_t A;
	int8_t K;
	int8_t mol;
	int8_t cd;
} sl_unit;

const sl_unit unit_V = { 2,  1, -3, -1,  0,  0,  0};
const sl_unit unit_A = { 0,  0,  0,  1,  0,  0,  0};

typedef struct sl_signal_info {
	sl_type type;

	const char* label;

	/// Bitmask of modes for which this signal is enabled as input
	uint32_t inputModes;

	/// Bitmask of modes for which this signal is enabled as output
	uint32_t outputModes;

	sl_unit unit;
	double min;
	double max;
	double resolution;
} sl_signal_info;

typedef struct sl_channel_info {
	sl_type type;
	const char* label;
	size_t mode_count;
	size_t signal_count;
} sl_channel_info;

typedef struct sl_device_info {
	sl_type type;
	const char* label;
	size_t channel_count;
} sl_device_info;

enum Dest {
	DEST_NONE,
	DEST_BUFFER,
	DEST_CALLBACK,
};

enum Src {
	SRC_CONSTANT,
	SRC_SQUARE,
	SRC_SAWTOOTH,
	SRC_STAIRSTEP,
	SRC_SINE,
	SRC_TRIANGLE,
	SRC_BUFFER,
	SRC_CALLBACK,
};

enum Modes {
	DISABLED,
	SVMI,
	SIMV,
};

namespace smu {
	/// @brief Generic signal class.
	class Signal {
	public:
		/// internal: Do not call the constructor directly; obtain a Signal from a Device
		Signal(const sl_signal_info* info): m_info(info), m_src(SRC_CONSTANT), m_src_v1(0), m_dest(DEST_NONE) {}
		~Signal();

		/// Get the descriptor struct of the Signal.
		/// Pointed-to memory is valid for the lifetime of the Device.
		const sl_signal_info* info() const { return m_info; }
		const sl_signal_info* const m_info;

		void source_constant(float val);

		void source_square(float midpoint, float peak, double period, double duty, double phase);

		void source_sawtooth(float midpoint, float peak, double period, double phase);

		void source_stairstep(float midpoint, float peak, double period, double phase);

		void source_sine(float midpoint, float peak, double period, double phase);

		void source_triangle(float midpoint, float peak, double period, double phase);

		void source_buffer(float* buf, size_t len, bool repeat);

		void source_callback(std::function<float (uint64_t index)> callback);

		/// Get the last measured sample from this signal.
		float measure_instantaneous() { return m_latest_measurement; }

		/// Configure received samples to be stored into `buf`, up to `len` points.
		/// After `len` points, samples will be dropped.
		void measure_buffer(float* buf, size_t len);

		/// Configure received samples to be passed to the provided callback.
		void measure_callback(std::function<void(float value)> callback);

		/// internal: Called by Device
		void put_sample(float val);

		/// internal: Called by Device
		float get_sample();

		void update_phase(double new_period, double new_phase);

		Src m_src;
		float m_src_v1;
		float m_src_v2;
		double m_src_period;
		double m_src_duty;
		double m_src_phase;

		float* m_src_buf = NULL;
		size_t m_src_i;
		size_t m_src_buf_len;
		bool m_src_buf_repeat;

		std::function<float (uint64_t index)> m_src_callback;

		Dest m_dest;

		// valid if m_dest == DEST_BUF
		float* m_dest_buf;
		size_t m_dest_buf_len;

		// valid if m_dest == DEST_CALLBACK
		std::function<void(float val)> m_dest_callback;

	protected:
		float m_latest_measurement;
	};
}
