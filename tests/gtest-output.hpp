// Custom output for the google test framework.
//
// Use PRINTF() where printf() would be used and TEST_COUT where cout would be
// used in order to match regular gtest output for custom status messages.

#include <gtest/gtest.h>

#include <stdarg.h>

enum GTestColor {
	COLOR_DEFAULT,
	COLOR_RED,
	COLOR_GREEN,
	COLOR_YELLOW
};

const char* GetAnsiColorCode(GTestColor color) {
	switch (color) {
		case COLOR_RED:		return "1";
		case COLOR_GREEN:	return "2";
		case COLOR_YELLOW:	return "3";
		default:			return NULL;
	};
}

void ColoredPrintf(GTestColor color, const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);

	static const bool in_color_mode = true;
	const bool use_color = in_color_mode && (color != COLOR_DEFAULT);

	if (!use_color) {
		vprintf(fmt, args);
		va_end(args);
		return;
	}

	printf("\033[0;3%sm", GetAnsiColorCode(color));
	vprintf(fmt, args);
	printf("\033[m"); // Resets the terminal to default.
	va_end(args);
}

#define PRINTF(...)  do { ColoredPrintf(COLOR_GREEN, "[**********] "); ColoredPrintf(COLOR_YELLOW, __VA_ARGS__); } while(0)

// C++ stream interface
class TestCout : public std::stringstream {
	public:
		~TestCout() {
			PRINTF("%s",str().c_str());
		}
};

#define TEST_COUT TestCout()
