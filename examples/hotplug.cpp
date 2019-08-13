// Simple example demonstrating hotplug support.

#include <chrono>
#include <iostream>
#include <thread>
#include <algorithm>

#include <libsmu/libsmu.hpp>

using std::cout;
using std::cerr;
using std::endl;

using namespace smu;

int main(int argc, char **argv)
{
	// Create session object and add all compatible devices them to the
	// session. Note that this currently doesn't handle returned errors.
	Session* session = new Session();
	std::vector < Device* > last_devices;

	while (true) {
		std::this_thread::sleep_for(std::chrono::milliseconds(2000));
		session->scan();

		std::vector < Device* > available_devices = session->m_available_devices;

		//check if there is any disconnected device
		for (auto other_device : last_devices) {
			bool found = 0;

			for (auto device : available_devices) {
				if (!device->m_serial.compare(other_device->m_serial)) {
					found = 1;
					break;
				}
			}

			if (!found) {
				cout << "Device detached!\n";
				last_devices.erase(std::remove(last_devices.begin(), last_devices.end(), other_device), last_devices.end());
			}
		}

		//check if there is any new connected device
		for (auto device : available_devices) {
			bool found = 0;

			for (auto other_device : last_devices) {
				if (!device->m_serial.compare(other_device->m_serial)) {
					found = 1;
					break;
				}
			}

			if (!found) {
				cout << "Device attached!\n";
			}
			else {
				available_devices.erase(std::remove(available_devices.begin(), available_devices.end(), device), available_devices.end());
				delete device;
			}
		}

		last_devices.insert(last_devices.end(), available_devices.begin(), available_devices.end());
		cout << "Number of available devices: " << last_devices.size() << '\n';
	}
}
