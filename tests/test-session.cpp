// Tests for session functionality.

#include <gtest/gtest.h>

#include "fixtures.hpp"
#include <libsmu/libsmu.hpp>

using namespace smu;

class SessionTest : public SessionFixture {};


TEST_F(SessionTest, empty) {
	// No devices on session init.
	EXPECT_EQ(m_session->m_devices.size(), 0);
}

int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
