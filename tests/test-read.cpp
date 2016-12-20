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
	m_session->flush();

	// Perform a non-continuous run/read session for a given amount of samples and time.
	auto run_read = [&](int run_samples, int read_samples, int max_run_time) {
		auto clk_start = std::chrono::high_resolution_clock::now();
		while (true) {
			auto clk_end = std::chrono::high_resolution_clock::now();
			auto clk_diff = std::chrono::duration_cast<std::chrono::seconds>(clk_end - clk_start);
			if (clk_diff.count() > max_run_time)
				break;

			m_session->run(run_samples);
			m_dev->read(rxbuf, read_samples, -1);
			EXPECT_EQ(rxbuf.size(), read_samples);
		}
	};

	// Unbalanced run/read calls will lead to sample drops.
	ASSERT_THROW(run_read(2000, 1000, 5), std::system_error);
	m_session->flush();

	// Run/read calls that aren't aligned to sample packet size won't lead to sample drops.
	ASSERT_NO_THROW(run_read(2000, 2000, 5));
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
	} catch (const std::runtime_error&) {
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
	} catch (const std::runtime_error&) {
		// ignore sample drops
	}

	// Which should be long enough to get all 1000 samples.
	EXPECT_EQ(rxbuf.size(), 1000);
}

TEST_F(ReadTest, continuous_sample_rates) {
	// Verify streaming HI-Z data values from 100 kSPS to 10 kSPS every ~5k SPS.
	// Run each session for a minute.
	unsigned test_ms = 60000;
	std::vector<float> failure_vals;
	std::vector<uint64_t> failure_samples;

	for (auto i = 100; i >= 10; i -= 5) {
		uint64_t sample_count = 0;
		bool failure = false;

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
			} catch (const std::runtime_error&) {
				// ignore sample drops
			}

			// Which all should be near 0.
			for (unsigned i = 0; i < rxbuf.size(); i++) {
				sample_count++;
				for (unsigned j = 0; j < 4; j++) {
					if (std::fabs(std::round(rxbuf[i][j])) != 0) {
						failure = true;
						failure_vals.push_back(rxbuf[i][j]);
						failure_samples.push_back(sample_count);
					}
				}

				// show output progress per second
				if (sample_count % sample_rate == 0) {
					if (failure) {
						failure = false;
						std::cout << "#" << std::flush;
					} else {
						std::cout << "*" << std::flush;
					}
				}
			}
		}
		std::cout << std::endl;

		// check if bad sample values were received and display them if any exist
		EXPECT_EQ(failure_samples.size(), 0);
		if (failure_samples.size() != 0) {
			std::cout << failure_samples.size() << " bad sample(s):" << std::endl;
			for (unsigned i = 0; i < failure_samples.size(); i++) {
				std::cout << "sample: " << failure_samples[i] << ", expected: 0, received: " << failure_vals[i] << std::endl;
			}
		}

		failure_samples.clear();
		failure_vals.clear();

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
