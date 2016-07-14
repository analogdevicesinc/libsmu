// Simple test for reading data from multiple devices.

#include <csignal>
#include <array>
#include <chrono>
#include <iostream>
#include <vector>
#include <system_error>
#include <thread>

#include <libsmu/libsmu.hpp>

using std::cout;
using std::cerr;
using std::endl;

using namespace smu;

void signalHandler(int signum)
{
	cerr << endl << "sleeping for a bit to cause an overflow exception" << endl;
	std::this_thread::sleep_for(std::chrono::milliseconds(250));
}

int main(int argc, char **argv)
{
	// Make SIGQUIT force sample drops.
	signal(SIGQUIT, signalHandler);

	// Create session object and scan system for compatible devices then add
	// them to the session. Note that this currently doesn't handle returned
	// errors.
	Session* session = new Session();
	session->scan();
	session->add_all();

	if (session->m_devices.size() < 2) {
		cerr << "This test is for multiple device setups, plug in more devices." << endl;
		exit(1);
	}

	// Grab the first device from the session.
	auto dev = *(session->m_devices.begin());

	// Run session at the default device rate.
	session->configure(dev->get_default_rate());

	// Run session in continuous mode.
	session->start(0);

	// Data to be read from the device is formatted into a vector of four
	// floats in an array, specifically in the format
	// <Chan A voltage, Chan A current, Chan B coltage, Chan B current>.
	std::vector<std::array<float, 4>> buf;
	float v;
	int i;

	while (true) {
		i = 0;
		for (auto dev: session->m_devices) {
			try {
				// Read 1024 samples at a time from the device.
				// Note that the timeout (3rd parameter to read() defaults to 0
				// (nonblocking mode) so the number of samples returned won't
				// necessarily be 1024.
				dev->read(buf, 1024);
			} catch (const std::system_error& e) {
				// Exit on dropped samples.
				cerr << "sample(s) dropped!" << endl;
				exit(1);
			}

			// Iterate over all returned samples, doesn't have to be 1024.
			for (auto a: buf) {
				v = a[0] + a[1] + a[2] + a[3];
				// Can force samples to drop on slower setups and/or slower
				// terminals, redirect stdout to alleviate this.
				printf("device %i: %f\n", i, v);
			}
			i++;
		}
	};
}
