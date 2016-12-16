// Test fixtures.

#include <gtest/gtest.h>

#include <libsmu/libsmu.hpp>

using namespace smu;

class SessionTest : public testing::Test {
	protected:
	// SetUp() is run immediately before a test starts.
	virtual void SetUp() {
		m_session = new Session();
	}

	// TearDown() is invoked immediately after a test finishes.
	virtual void TearDown() {
		delete m_session;
	}

	Session* m_session;
};
