// Tests for read/write functionality.

#include <gtest/gtest.h>

#include <cmath>
#include <array>
#include <chrono>
#include <thread>
#include <vector>

#include "fixtures.hpp"
#include <libsmu/libsmu.hpp>

using namespace smu;

class ReadWriteTest : public SingleDeviceFixture {};

// refill Tx buffers with data
static void refill_data(std::vector<float>& buf, unsigned size, int voltage) {
	buf.clear();
	for (unsigned i = 0; i < size; i++) {
		buf.push_back(voltage);
	}
}

TEST_F(ReadWriteTest, non_continuous) {
	// Set device channels to source voltage and measure current.
	m_dev->set_mode(0, SVMI);
	m_dev->set_mode(1, SVMI);

	std::vector<std::array<float, 4>> rxbuf;
	std::vector<float> a_txbuf;
	std::vector<float> b_txbuf;

	// Verify read/write data for 10 seconds.
	unsigned voltage = 0;
	uint64_t sample_count = 0;
	auto clk_start = std::chrono::high_resolution_clock::now();
	while (true) {
		auto clk_end = std::chrono::high_resolution_clock::now();
		auto clk_diff = std::chrono::duration_cast<std::chrono::seconds>(clk_end - clk_start);
		if (clk_diff.count() > 10)
			break;

		// Refill outgoing data buffers.
		refill_data(a_txbuf, 1024, voltage % 6);
		refill_data(b_txbuf, 1024, voltage % 6);

		// Write iterating voltage values to both channels.
		m_dev->write(a_txbuf, 0);
		m_dev->write(b_txbuf, 1);

		// Run the session for 1024 samples.
		m_session->run(1024);

		// Read incoming samples in a blocking fashion.
		m_dev->read(rxbuf, 1024, -1);

		// Validate received values.
		EXPECT_EQ(rxbuf.size(), 1024);
		for (unsigned i = 0; i < rxbuf.size(); i++) {
			sample_count++;
			EXPECT_EQ(voltage % 6, std::fabs(std::round(rxbuf[i][0]))) << "failed at sample: " << sample_count;
			EXPECT_EQ(voltage % 6, std::fabs(std::round(rxbuf[i][2]))) << "failed at sample: " << sample_count;
		}
		voltage++;
	}
}

int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
