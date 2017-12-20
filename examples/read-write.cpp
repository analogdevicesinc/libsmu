// Simple example for reading/writing data in a non-continuous fashion.

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include <array>
#include <chrono>
#include <functional>
#include <iostream>
#include <vector>
#include <system_error>
#include <thread>

#include <libsmu/libsmu.hpp>

#ifndef _WIN32
int (*_isatty)(int) = &isatty;
#endif

using std::cout;
using std::cerr;
using std::endl;

using namespace smu;

int main(int argc, char **argv)
{
	// Create session object and add all compatible devices them to the
	// session. Note that this currently doesn't handle returned errors.
	Session* session = new Session();
	session->add_all();

	if (session->m_devices.size() == 0) {
		cerr << "Plug in a device." << endl;
		exit(1);
	}

	// Grab the first device from the session.
	auto dev = *(session->m_devices.begin());

	// Set device channels to source voltage and measure current.
	dev->set_mode(0, SVMI);
	dev->set_mode(1, SVMI);

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

	uint64_t i = 0;
	uint64_t v = 0;

	while (true) {
		// If stdout is a terminal change the written value approximately
		// every second, otherwise change it on every iteration.
		if (_isatty(1))
			v = i / 100;
		else
			v = i;

		// Refill outgoing data buffers.
		refill_data(a_txbuf, 1024, v % 6);
		refill_data(b_txbuf, 1024, v % 6);
		i++;

		try {
			// Write iterating voltage values to both channels.
			dev->write(a_txbuf, 0);
			dev->write(b_txbuf, 1);

			// Run the session for 1024 samples.
			session->run(1024);

			// Read incoming samples in a blocking fashion.
			dev->read(rxbuf, 1024, -1);
		} catch (const std::system_error& e) {
			if (!_isatty(1)) {
				// Exit on dropped samples when stdout isn't a terminal.
				cerr << "sample(s) dropped: " << e.what() << endl;
				exit(1);
			}
		}

		for (auto i: rxbuf) {
			// Overwrite a singular line if stdout is a terminal, otherwise
			// output line by line.
			if (_isatty(1))
				printf("\r% 6f % 6f % 6f % 6f", i[0], i[1], i[2], i[3]);
			else
				printf("% 6f % 6f % 6f % 6f\n", i[0], i[1], i[2], i[3]);
		}
	};
}
