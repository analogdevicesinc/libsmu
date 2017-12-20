// Simple example demonstrating hotplug support.

#include <chrono>
#include <iostream>
#include <thread>

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
	session->add_all();

	// Register hotplug detach callback handler.
	session->hotplug_detach([=](Device* device, void* data){
		session->cancel();
		if (!session->remove(device, true)) {
			printf("device detached: %s: serial %s: fw %s: hw %s\n",
				device->info()->label, device->m_serial.c_str(),
				device->m_fwver.c_str(), device->m_hwver.c_str());
		}
	});

	// Register hotplug attach callback handler.
	session->hotplug_attach([=](Device* device, void* data){
		if (!session->add(device)) {
			printf("device attached: %s: serial %s: fw %s: hw %s\n",
				device->info()->label, device->m_serial.c_str(),
				device->m_fwver.c_str(), device->m_hwver.c_str());
		}
	});

	cout << "waiting for hotplug events..." << endl;
	while (true)
		std::this_thread::sleep_for(std::chrono::seconds(1));
}
