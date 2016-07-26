// Simple test for hotplug support.

#include <chrono>
#include <iostream>
#include <thread>

#include <libsmu/libsmu.hpp>

using std::cout;
using std::cerr;
using std::endl;

using namespace smu;

static bool detached, attached;

int main(int argc, char **argv)
{
	detached = attached = false;

	// Create session object and add all compatible devices them to the
	// session. Note that this currently doesn't handle returned errors.
	Session* session = new Session();
	session->add_all();

	// Register hotplug detach callback handler.
	session->m_hotplug_detach_callback = [=](Device* dev){
		session->cancel();
		session->remove(dev);
		printf("removed device: %s: serial %s: fw %s: hw %s\n",
				dev->info()->label, dev->serial(),
				dev->fwver(), dev->hwver());
		detached = true;
	};

	// Register hotplug attach callback handler.
	session->m_hotplug_attach_callback = [=](Device* dev){
		if (session->add(dev))
			printf("added device: %s: serial %s: fw %s: hw %s\n",
				dev->info()->label, dev->serial(),
				dev->fwver(), dev->hwver());
		attached = true;
	};

	// Wait around doing nothing until both detach and attach events are fired.
	// TODO: force all initially attached devices to be detached and reattached
	// for fixing/testing device ordering within a session on hotplug.
	while (!detached && !attached)
		std::this_thread::sleep_for(std::chrono::seconds(1));
}
