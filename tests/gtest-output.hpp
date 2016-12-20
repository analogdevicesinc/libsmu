// Custom output for the google test framework.
// http://stackoverflow.com/questions/16491675/how-to-send-custom-message-in-google-c-testing-framework
//
// Use PRINTF() where printf() would be used and TEST_COUT where cout would be
// used in order to match regular gtest output for custom status messages.

#include <gtest/gtest.h>

namespace testing {
	namespace internal {
		enum GTestColor {
			COLOR_DEFAULT,
			COLOR_RED,
			COLOR_GREEN,
			COLOR_YELLOW
		};

		extern void ColoredPrintf(GTestColor color, const char* fmt, ...);
	}
}

#define PRINTF(...)  do { testing::internal::ColoredPrintf(testing::internal::COLOR_GREEN, "[          ] "); testing::internal::ColoredPrintf(testing::internal::COLOR_YELLOW, __VA_ARGS__); } while(0)

// C++ stream interface
class TestCout : public std::stringstream {
	public:
		~TestCout() {
			PRINTF("%s",str().c_str());
		}
};

#define TEST_COUT TestCout()
