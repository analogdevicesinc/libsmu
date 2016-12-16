// Tests for device functionality.

#include <gtest/gtest.h>

#include "fixtures.hpp"
#include <libsmu/libsmu.hpp>

using namespace smu;

class DeviceTest : public SingleDeviceFixture {};


TEST_F(DeviceTest, serial) {
	ASSERT_NE(m_dev->m_serial, "");
}

TEST_F(DeviceTest, fwver) {
	ASSERT_NE(m_dev->m_fwver, "");
}

TEST_F(DeviceTest, hwver) {
	ASSERT_NE(m_dev->m_hwver, "");
}

int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
