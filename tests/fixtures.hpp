// Test fixtures.

#include <gtest/gtest.h>

#include <set>

#include <libsmu/libsmu.hpp>

using namespace smu;

// Create a session.
class SessionFixture : public testing::Test {
	protected:
		Session* m_session;

		// SetUp() is run immediately before a test starts.
		virtual void SetUp() {
			m_session = new Session();
		}

		// TearDown() is invoked immediately after a test finishes.
		virtual void TearDown() {
			delete m_session;
		}
};

// Require at least one device to exist.
class SingleDeviceFixture : public SessionFixture {
	protected:
		Device* m_dev;

		virtual void SetUp() {
			SessionFixture::SetUp();
			m_session->scan();

			// requires at least one device plugged in
			if (m_session->m_available_devices.size() == 0)
				FAIL() << "no devices plugged in";

			m_session->add(m_session->m_available_devices[0]);
			m_dev = *(m_session->m_devices.begin());
		}
};

// Require at least two devices to exist.
class MultiDeviceFixture : public SessionFixture {
	protected:
		std::set<Device*> m_devices;

		virtual void SetUp() {
			SessionFixture::SetUp();
			m_session->add_all();

			// requires at least one device plugged in
			if (m_session->m_devices.size() < 2)
				FAIL() << "multiple devices are required";

			m_devices = m_session->m_devices;
		}
};
