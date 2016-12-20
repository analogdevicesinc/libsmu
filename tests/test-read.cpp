// Tests for read functionality.

#include <gtest/gtest.h>

#include <cmath>
#include <array>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "fixtures.hpp"
#include "gtest-output.hpp"
#include <libsmu/libsmu.hpp>

using namespace smu;

class ReadTest : public SingleDeviceFixture {
	protected:
		std::vector<std::array<float, 4>> rxbuf;

		// TearDown() is invoked immediately after a test finishes.
		virtual void TearDown() {
			SingleDeviceFixture::TearDown();
			rxbuf.clear();
		}
};

TEST_F(ReadTest, non_continuous) {
	// Run session in non-continuous mode.
	m_session->run(1000);

	// Grab 1000 samples in a blocking fashion in HI-Z mode.
	m_dev->read(rxbuf, 1000, -1);

	// We should have gotten 1000 samples.
	EXPECT_EQ(rxbuf.size(), 1000);

	// Which all should be near 0.
	for (unsigned i = 0; i < rxbuf.size(); i++) {
		EXPECT_EQ(0, std::fabs(std::round(rxbuf[i][0]))) << "failed at sample " << i;
		EXPECT_EQ(0, std::fabs(std::round(rxbuf[i][1]))) << "failed at sample " << i;
		EXPECT_EQ(0, std::fabs(std::round(rxbuf[i][2]))) << "failed at sample " << i;
		EXPECT_EQ(0, std::fabs(std::round(rxbuf[i][3]))) << "failed at sample " << i;
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
			EXPECT_EQ(0, std::fabs(std::round(rxbuf[i][0]))) << "failed at sample: " << sample_count;
			EXPECT_EQ(0, std::fabs(std::round(rxbuf[i][1]))) << "failed at sample: " << sample_count;
			EXPECT_EQ(0, std::fabs(std::round(rxbuf[i][2]))) << "failed at sample: " << sample_count;
			EXPECT_EQ(0, std::fabs(std::round(rxbuf[i][3]))) << "failed at sample: " << sample_count;
		}
	}
}

TEST_F(ReadTest, non_continuous_sample_drop) {
	// Run the session for more samples than the incoming queue fits.
	m_session->run(m_session->m_queue_size + 1);

	// Trying to read should now throw a sample drop exception.
	ASSERT_THROW(m_dev->read(rxbuf, 1000), std::system_error);

	// Unbalanced run/read calls will lead to sample drops.
	auto unbalanced_run_read = [&](int run_samples, int read_samples) {
		auto clk_start = std::chrono::high_resolution_clock::now();
		while (true) {
			auto clk_end = std::chrono::high_resolution_clock::now();
			auto clk_diff = std::chrono::duration_cast<std::chrono::seconds>(clk_end - clk_start);
			if (clk_diff.count() > 3)
				break;

			m_session->run(run_samples);
			m_dev->read(rxbuf, read_samples, -1);
			EXPECT_EQ(rxbuf.size(), read_samples);
		}
	};

	ASSERT_THROW(unbalanced_run_read(1024, 1000), std::system_error);
}

TEST_F(ReadTest, continuous_sample_drop) {
	// Run session in continuous mode.
	m_session->start(0);

	// Sleeping for a bit to cause a sample drop exception.
	std::this_thread::sleep_for(std::chrono::milliseconds(250));

	// Trying to read should now throw a sample drop exception.
	ASSERT_THROW(m_dev->read(rxbuf, 1000), std::system_error);
}

TEST_F(ReadTest, continuous_non_blocking) {
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
}

TEST_F(ReadTest, continuous_blocking) {
	// Run session in continuous mode.
	m_session->start(0);

	// Grab 1000 samples in a blocking fashion.
	try {
		m_dev->read(rxbuf, 1000, -1);
	} catch (const std::runtime_error& e) {
		// ignore sample drops
	}

	EXPECT_EQ(rxbuf.size(), 1000);
}

TEST_F(ReadTest, continuous_timeout) {
	// Run session in continuous mode.
	m_session->start(0);

	// Grab 1000 samples with a timeout of 110ms.
	try {
		m_dev->read(rxbuf, 1000, 110);
	} catch (const std::runtime_error& e) {
		// ignore sample drops
	}

	// Which should be long enough to get all 1000 samples.
	EXPECT_EQ(rxbuf.size(), 1000);
}

TEST_F(ReadTest, continuous_sample_rates) {
	// Verify streaming HI-Z data values from 100 kSPS to 10 kSPS every ~5k SPS.
	// Run each session for a minute.
	unsigned test_ms = 60000;
	for (auto i = 100; i >= 10; i -= 5) {
		uint64_t sample_count = 0;
		auto clk_start = std::chrono::high_resolution_clock::now();
		auto clk_end = std::chrono::high_resolution_clock::now();
		auto clk_diff = std::chrono::duration_cast<std::chrono::milliseconds>(clk_end - clk_start);

		int sample_rate = m_session->configure(i * 1000);
		// Make sure the session got configured properly.
		EXPECT_GT(sample_rate, 0) << "failed to configure session at " << sample_rate << " SPS";
		// Verify we're within the minimum configurable range from the specified target.
		EXPECT_LE(std::abs((i * 1000) - sample_rate), 256);
		TEST_COUT << "running test at " << sample_rate << " SPS" << std::endl;
		m_session->start(0);

		while (true) {
			clk_end = std::chrono::high_resolution_clock::now();
			clk_diff = std::chrono::duration_cast<std::chrono::milliseconds>(clk_end - clk_start);
			if (clk_diff.count() > test_ms) {
				break;
			}

			// Grab 1000 samples in a non-blocking fashion in HI-Z mode.
			try {
				m_dev->read(rxbuf, 1000);
			} catch (const std::runtime_error& e) {
				// ignore sample drops
			}

			// Which all should be near 0.
			for (unsigned i = 0; i < rxbuf.size(); i++) {
				sample_count++;
				EXPECT_EQ(0, std::fabs(std::round(rxbuf[i][0]))) << "failed at sample: " << sample_count;
				EXPECT_EQ(0, std::fabs(std::round(rxbuf[i][1]))) << "failed at sample: " << sample_count;
				EXPECT_EQ(0, std::fabs(std::round(rxbuf[i][2]))) << "failed at sample: " << sample_count;
				EXPECT_EQ(0, std::fabs(std::round(rxbuf[i][3]))) << "failed at sample: " << sample_count;
			}
		}

		int samples_per_second = std::round((float)sample_count / (clk_diff.count() / 1000));
		// Verify we're running within 250 SPS of the configured sample rate.
		EXPECT_LE(std::abs(samples_per_second - sample_rate), 250);
		PRINTF("received %lu samples in %lu seconds: ~%u SPS\n", sample_count, clk_diff.count() / 1000, samples_per_second);

		// Stop the session.
		m_session->cancel();
		m_session->end();
	}
}

int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
