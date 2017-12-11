// Simple example for setting devices leds.

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include <array>
#include <chrono>
#include <iostream>
#include <vector>
#include <system_error>
#include <thread>

#include <libsmu/libsmu.hpp>

#ifndef _WIN32
int (*_isatty)(int) = &isatty;
#endif

using std::cerr;
using std::endl;

using namespace smu;

int main(int argc, char **argv)
{
    Session* session = new Session();
    session->add_all();

    if (session->m_devices.size() == 0) {
        cerr << "Plug in a device." << endl;
        exit(1);
    }
	
	while(true){
		for(unsigned i=0; i<8;i++){
			for(auto dev : session->m_devices){
				dev->set_led(i);
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(250));
		}
	}
	return 0;
}
