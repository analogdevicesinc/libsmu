// Test for reading data.

#include <gtest/gtest.h>

#include <array>
#include <vector>

#include <libsmu/libsmu.hpp>

using namespace smu;

class SessionTest : public testing::Test {
	protected:
	// Remember that SetUp() is run immediately before a test starts.
	// This is a good place to record the start time.
	virtual void SetUp() {
		m_session = new Session();
		m_session->add_all();
	}

	// TearDown() is invoked immediately after a test finishes.  Here we
	// check if the test was too slow.
	virtual void TearDown() {
		delete m_session;
	}

	Session* m_session;
};

// Derive a fixture named ReadTest from the Session fixture.
class ReadTest : public SessionTest {
  // We don't need any more logic than already in the Session fixture.
  // Therefore the body is empty.
};

TEST_F(ReadTest, Read) {
	// Grab the first device from the session.
	auto dev = *(m_session->m_devices.begin());

	std::vector<std::array<float, 4>> rxbuf;

	// Run session in non-continuous mode.
	m_session->run(1000);

	dev->read(rxbuf, 1000, -1);
	EXPECT_EQ(1000, rxbuf.size());
}

int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
