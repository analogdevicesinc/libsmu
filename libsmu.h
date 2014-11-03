#include <stdint.h>
#include <stddef.h>

struct sl_session;
struct sl_device;
struct sl_signal;

typedef enum sl_type {
	DEVICE_CEE_1 = 0x10000,
	DEVICE_M1000,
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

typedef float value_t;
typedef uint64_t sample_t;
