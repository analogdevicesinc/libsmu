// Tests for read functionality.

#include <gtest/gtest.h>

#include <cmath>
#include <array>
#include <thread>
#include <vector>

#include "fixtures.hpp"
#include <libsmu/libsmu.hpp>

using namespace smu;

class MultiReadTest : public MultiDeviceFixture {};

TEST_F(MultiReadTest, non_continuous) {
	std::vector<std::array<float, 4>> rxbuf;

	// Run session in non-continuous mode.
	m_session->run(1000);

	for (auto dev: m_devices) {
		// Grab 1000 samples in a blocking fashion in HI-Z mode.
		dev->read(rxbuf, 1000, -1);

		// We should have gotten 1000 samples.
		EXPECT_EQ(rxbuf.size(), 1000);

		// Which all should be near 0.
		for (auto x: rxbuf) {
			EXPECT_EQ(0, fabs(round(x[0])));
			EXPECT_EQ(0, fabs(round(x[1])));
			EXPECT_EQ(0, fabs(round(x[2])));
			EXPECT_EQ(0, fabs(round(x[3])));
		}
	}
}

TEST_F(MultiReadTest, continuous) {
	std::vector<std::array<float, 4>> rxbuf;

	// Run session in continuous mode.
	m_session->start(0);

	for (auto dev: m_devices) {
		// Grab 1000 samples in a nonblocking fashion in HI-Z mode.
		dev->read(rxbuf, 1000);

		// We should have gotten between 0 and 1000 samples.
		EXPECT_LE(rxbuf.size(), 1000);
		EXPECT_GE(rxbuf.size(), 0);
		rxbuf.clear();

		// Grab 1000 samples with a timeout of 150ms.
		dev->read(rxbuf, 1000, 150);
		// Which should be long enough to get all 1000 samples.
		EXPECT_EQ(rxbuf.size(), 1000);
		rxbuf.clear();

		// Grab 1000 more samples in a blocking fashion.
		dev->read(rxbuf, 1000, -1);
		EXPECT_EQ(rxbuf.size(), 1000);
		rxbuf.clear();

		// Sleeping for a bit to cause an overflow exception.
		std::this_thread::sleep_for(std::chrono::milliseconds(250));

		// Trying to read should now throw a buffer overflow exception.
		ASSERT_THROW(dev->read(rxbuf, 1000), std::system_error);
	}
}


int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
