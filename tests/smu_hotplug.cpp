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
	session->hotplug_detach([=](Device* device, void* data){
		session->cancel();
		if (!session->remove(device, true)) {
			printf("removed device: %s: serial %s: fw %s: hw %s\n",
				device->info()->label, device->serial(),
				device->fwver(), device->hwver());
			detached = true;
		}
	});

	// Register hotplug attach callback handler.
	session->hotplug_attach([=](Device* device, void* data){
		if (!session->add(device)) {
			printf("added device: %s: serial %s: fw %s: hw %s\n",
				device->info()->label, device->serial(),
				device->fwver(), device->hwver());
			attached = true;
		}
	});

	// Wait around doing nothing until both detach and attach events are fired.
	// TODO: force all initially attached devices to be detached and reattached
	// for fixing/testing device ordering within a session on hotplug.
	while (!(detached && attached))
		std::this_thread::sleep_for(std::chrono::seconds(1));
}
