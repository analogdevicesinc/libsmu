// Simple test for writing data.

#include <csignal>
#include <array>
#include <chrono>
#include <iostream>
#include <vector>
#include <random>
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

	// Run session at the default device rate.
	session->configure(dev->get_default_rate());

	// Run session in continuous mode.
	session->start(0);

	std::vector<std::array<float, 4>> rxbuf;
	std::vector<float> a_txbuf;
	std::vector<float> b_txbuf;

	// Write static, random integers between 0 and 5 to voltage channels.
	std::random_device r;
	std::default_random_engine rand_eng(r());
	std::uniform_int_distribution<int> rand_v(0, 5);

	// refill Tx buffers with data
	std::function<void(std::vector<float>& buf, unsigned size, int voltage)> refill_data;
	refill_data = [=](std::vector<float>& buf, unsigned size, int voltage) {
		buf.clear();
		for (unsigned i = 0; i < size; i++) {
			buf.push_back(voltage);
		}
	};

	while (true) {
		refill_data(a_txbuf, 10000, rand_v(rand_eng));
		refill_data(b_txbuf, 10000, rand_v(rand_eng));
		try {
			dev->write(a_txbuf, 0);
			dev->write(b_txbuf, 1);
			dev->read(rxbuf, 10000);
		} catch (const std::system_error& e) {
			// Exit on dropped samples.
			cerr << "sample(s) dropped: " << e.what() << endl;
			exit(1);
		}

		for (auto i: rxbuf) {
			printf("%f %f %f %f\n", i[0], i[1], i[2], i[3]);
		}
	};
}
