// Tests for read functionality.

#include <gtest/gtest.h>

#include <cmath>
#include <array>
#include <chrono>
#include <thread>
#include <vector>

#include "fixtures.hpp"
#include <libsmu/libsmu.hpp>

using namespace smu;

class ReadTest : public SingleDeviceFixture {};

TEST_F(ReadTest, non_continuous) {
	std::vector<std::array<float, 4>> rxbuf;

	// Run session in non-continuous mode.
	m_session->run(1000);

	// Grab 1000 samples in a blocking fashion in HI-Z mode.
	m_dev->read(rxbuf, 1000, -1);

	// We should have gotten 1000 samples.
	EXPECT_EQ(rxbuf.size(), 1000);

	// Which all should be near 0.
	for (unsigned i = 0; i < rxbuf.size(); i++) {
		EXPECT_EQ(0, fabs(round(rxbuf[i][0]))) << "failed at sample " << i;
		EXPECT_EQ(0, fabs(round(rxbuf[i][1]))) << "failed at sample " << i;
		EXPECT_EQ(0, fabs(round(rxbuf[i][2]))) << "failed at sample " << i;
		EXPECT_EQ(0, fabs(round(rxbuf[i][3]))) << "failed at sample " << i;
	}

	// Verify streaming HI-Z data values for ten seconds.
	uint64_t sample_count = 0;
	auto clk_start = std::chrono::high_resolution_clock::now();
	while (true) {
		auto clk_end = std::chrono::high_resolution_clock::now();
		auto clk_diff = std::chrono::duration_cast<std::chrono::seconds>(clk_end - clk_start);
		if (clk_diff.count() > 10)
			break;

		m_session->run(1024);
		m_dev->read(rxbuf, 1024, -1);
		EXPECT_EQ(rxbuf.size(), 1024);
		// Which all should be near 0.
		for (unsigned i = 0; i < rxbuf.size(); i++) {
			sample_count++;
			EXPECT_EQ(0, fabs(round(rxbuf[i][0]))) << "failed at sample: " << sample_count;
			EXPECT_EQ(0, fabs(round(rxbuf[i][1]))) << "failed at sample: " << sample_count;
			EXPECT_EQ(0, fabs(round(rxbuf[i][2]))) << "failed at sample: " << sample_count;
			EXPECT_EQ(0, fabs(round(rxbuf[i][3]))) << "failed at sample: " << sample_count;
		}
	}
}

TEST_F(ReadTest, continuous) {
	std::vector<std::array<float, 4>> rxbuf;

	// Try to get samples in a nonblocking fashion before a session is started.
	m_dev->read(rxbuf, 1000);
	// Shouldn't be an issue as long as you always expect 0 samples back.
	EXPECT_EQ(rxbuf.size(), 0);

	// Run session in continuous mode.
	m_session->start(0);

	// Grab 1000 samples in a nonblocking fashion in HI-Z mode.
	m_dev->read(rxbuf, 1000);

	// We should have gotten between 0 and 1000 samples.
	EXPECT_LE(rxbuf.size(), 1000);
	EXPECT_GE(rxbuf.size(), 0);
	rxbuf.clear();

	// Grab 1000 samples with a timeout of 150ms.
	m_dev->read(rxbuf, 1000, 150);
	// Which should be long enough to get all 1000 samples.
	EXPECT_EQ(rxbuf.size(), 1000);
	rxbuf.clear();

	// Grab 1000 more samples in a blocking fashion.
	m_dev->read(rxbuf, 1000, -1);
	EXPECT_EQ(rxbuf.size(), 1000);
	rxbuf.clear();

	// Sleeping for a bit to cause an overflow exception.
	std::this_thread::sleep_for(std::chrono::milliseconds(250));

	// Trying to read should now throw a buffer overflow exception.
	ASSERT_THROW(m_dev->read(rxbuf, 1000), std::system_error);
	m_session->flush();

	// Verify streaming HI-Z data values for a minute.
	uint64_t sample_count = 0;
	auto clk_start = std::chrono::high_resolution_clock::now();
	while (true) {
		auto clk_end = std::chrono::high_resolution_clock::now();
		auto clk_diff = std::chrono::duration_cast<std::chrono::seconds>(clk_end - clk_start);
		if (clk_diff.count() > 60)
			break;

		// Grab 1000 samples in a nonblocking fashion in HI-Z mode.
		try {
			m_dev->read(rxbuf, 1000);
		} catch (const std::runtime_error& e) {
			// ignore sample drops
		}
		// Which all should be near 0.
		for (unsigned i = 0; i < rxbuf.size(); i++) {
			sample_count++;
			EXPECT_EQ(0, fabs(round(rxbuf[i][0]))) << "failed at sample: " << sample_count;
			EXPECT_EQ(0, fabs(round(rxbuf[i][1]))) << "failed at sample: " << sample_count;
			EXPECT_EQ(0, fabs(round(rxbuf[i][2]))) << "failed at sample: " << sample_count;
			EXPECT_EQ(0, fabs(round(rxbuf[i][3]))) << "failed at sample: " << sample_count;
		}
	}


}


int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
