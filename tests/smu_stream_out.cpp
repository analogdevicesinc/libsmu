// Simple test for reading data without dropping samples.
//
// When run the test should output summed sample values from all channels to
// stdout in a continuous fashion. If any sample is dropped the program exits
// with an error code of 1.

#include <csignal>
#include <array>
#include <chrono>
#include <iostream>
#include <vector>
#include <system_error>
#include <thread>

#include <libsmu/libsmu.hpp>

using std::cout;
using std::endl;

using namespace smu;

void signalHandler(int signum)
{
	cout << endl << "sleeping for a bit to cause an overflow exception" << endl;
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

	// Grab the first device from the session (we're assuming one exists).
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

	while (true) {
		try {
			// Read 1024 samples at a time from the device.
			// Note that the timeout (3rd parameter to read() defaults to 0
			// (nonblocking mode) so the number of samples returned won't
			// necessarily be 1024.
			dev->read(buf, 1024);
		} catch (const std::system_error& e) {
			// Exit on dropped samples.
			fprintf(stderr, "sample(s) dropped!\n");
			exit(1);
		}

		// Iterate over all returned samples, doesn't have to be 1024).
		for (auto i: buf) {
			// Do something random but (probably) fast enough so samples aren't
			// dropped. This could depend on the terminal it gets run on due to
			// the printf() if stdout isn't redirected.
			v = i[0] + i[1] + i[2] + i[3];
			printf("%f\n", v);
		}
	};
}
