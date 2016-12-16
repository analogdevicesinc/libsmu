// Tests for read/write functionality.

#include <gtest/gtest.h>

#include <cmath>
#include <array>
#include <thread>
#include <vector>

#include "fixtures.hpp"
#include <libsmu/libsmu.hpp>

using namespace smu;

class ReadWriteTest : public SingleDeviceFixture {};

TEST_F(ReadWriteTest, non_continuous) {
	// Set device channels to source voltage and measure current.
	m_dev->set_mode(0, SVMI);
	m_dev->set_mode(1, SVMI);

	std::vector<std::array<float, 4>> rxbuf;
	std::vector<float> a_txbuf;
	std::vector<float> b_txbuf;

	// refill Tx buffers with data
	auto refill_data = [=](std::vector<float>& buf, unsigned size, int voltage) {
		buf.clear();
		for (unsigned i = 0; i < size; i++) {
			buf.push_back(voltage);
		}
	};

	for (auto v = 0; v <= 5; v++) {
		// Refill outgoing data buffers.
		refill_data(a_txbuf, 1024, v);
		refill_data(b_txbuf, 1024, v);

		// Write iterating voltage values to both channels.
		m_dev->write(a_txbuf, 0);
		m_dev->write(b_txbuf, 1);

		// Run the session for 1024 samples.
		m_session->run(1024);

		// Read incoming samples in a blocking fashion.
		m_dev->read(rxbuf, 1024, -1);

		// Validate received values.
		EXPECT_EQ(rxbuf.size(), 1024);
		for (auto x: rxbuf) {
			EXPECT_EQ(v, fabs(round(x[0])));
			EXPECT_EQ(v, fabs(round(x[2])));
		}
	}
}

int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
