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

class ReadWriteTest : public SingleDeviceFixture {
	protected:
		std::vector<std::array<float, 4>> rxbuf;
		std::vector<float> a_txbuf;
		std::vector<float> b_txbuf;
		uint64_t sample_count = 0;

		// TearDown() is invoked immediately after a test finishes.
		virtual void TearDown() {
			SingleDeviceFixture::TearDown();
			rxbuf.clear();
			a_txbuf.clear();
			b_txbuf.clear();
			sample_count = 0;
		}
};

// refill Tx buffers with data
static void refill_data(std::vector<float>& buf, unsigned size, int voltage) {
	buf.clear();
	for (unsigned i = 0; i < size; i++) {
		buf.push_back(voltage);
	}
}

TEST_F(ReadWriteTest, non_continuous_fallback_values) {
	// Set device channels to source voltage and measure current.
	m_dev->set_mode(0, SVMI);
	m_dev->set_mode(1, SVMI);


	// Fill Tx buffers with 1000 samples and request 1024 back. The remaining
	// 24 samples should have the same output values since the most recently
	// written value to the channel will be used to complete output packets.
	refill_data(a_txbuf, 1000, 2);
	refill_data(b_txbuf, 1000, 4);
	m_dev->write(a_txbuf, 0);
	m_dev->write(b_txbuf, 1);
	m_session->run(1024);
	m_dev->read(rxbuf, 1024, -1);

	// Verify all samples are what we expect.
	for (unsigned i = 0; i < rxbuf.size(); i++) {
		sample_count++;
		EXPECT_EQ(2, std::fabs(std::round(rxbuf[i][0]))) << "failed at sample: " << sample_count;
		EXPECT_EQ(4, std::fabs(std::round(rxbuf[i][2]))) << "failed at sample: " << sample_count;
	}
}

TEST_F(ReadWriteTest, non_continuous) {
	// Set device channels to source voltage and measure current.
	m_dev->set_mode(0, SVMI);
	m_dev->set_mode(1, SVMI);

	// Verify read/write data for 10 seconds.
	unsigned voltage = 0;
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

TEST_F(ReadWriteTest, continuous) {
	// Set device channels to source voltage and measure current.
	m_dev->set_mode(0, SVMI);
	m_dev->set_mode(1, SVMI);

	// Run session in continuous mode.
	m_session->start(0);

	// Verify read/write data for 10 seconds.
	unsigned voltage = 0;
	auto clk_start = std::chrono::high_resolution_clock::now();
	while (true) {
		auto clk_end = std::chrono::high_resolution_clock::now();
		auto clk_diff = std::chrono::duration_cast<std::chrono::seconds>(clk_end - clk_start);
		if (clk_diff.count() > 10)
			break;

		// Refill outgoing data buffers.
		refill_data(a_txbuf, 1000, voltage % 6);
		refill_data(b_txbuf, 1000, voltage % 6);

		try {
			// Write iterating voltage values to both channels.
			m_dev->write(a_txbuf, 0);
			m_dev->write(b_txbuf, 1);

			// Read incoming samples in a non-blocking fashion.
			m_dev->read(rxbuf, 1000);
		} catch (const std::runtime_error&) {
			// ignore sample drops
		}

		// Validate received values.
		for (unsigned i = 0; i < rxbuf.size(); i++) {
			sample_count++;
			EXPECT_EQ(voltage % 6, std::fabs(std::round(rxbuf[i][0]))) << "value: " << rxbuf[i][0] << " ,failed at sample: " << sample_count;
			EXPECT_EQ(voltage % 6, std::fabs(std::round(rxbuf[i][2]))) << "value: " << rxbuf[i][2] << " ,failed at sample: " << sample_count;
		}

		if (sample_count && sample_count % 1000 == 0)
			voltage++;
	}

	// Verify we're running at 100+ ksps.
	EXPECT_GE(sample_count, 100000*10);
}

int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
