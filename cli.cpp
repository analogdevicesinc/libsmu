// Released under the terms of the BSD License
// (C) 2014-2015
//   Analog Devices, Inc.
//   Kevin Mehall <km@kevinmehall.net>
//   Ian Daniher <itdaniher@gmail.com>

#include <memory>
#include "libsmu.hpp"
#include <iostream>
#include <vector>
#include <cstdint>
#include <string.h>
#include <unistd.h>

using std::cout;
using std::cerr;
using std::endl;
using std::unique_ptr;
using std::vector;
using std::string;

int main(int argc, char **argv) {
	int c;

	Session* session = new Session();
	session->m_completion_callback = [=](unsigned status){
	};

	session->m_progress_callback = [=](sample_t n) {
	};

	session->m_hotplug_detach_callback = [=](Device* device){
		session->cancel();
		session->remove_device(device);
	};

	session->m_hotplug_attach_callback = [=](Device* device){
		auto dev = session->add_device(device);
		cout << "added_device" << endl;
		auto dev_info = dev->info();
		for (unsigned ch_i=0; ch_i < dev_info->channel_count; ch_i++) {
			auto ch_info = dev->channel_info(ch_i);
			dev->set_mode(ch_i, 1);
			for (unsigned sig_i=0; sig_i < ch_info->signal_count; sig_i++) {
				auto sig = dev->signal(ch_i, sig_i);
				auto sig_info = sig->info();
				sig->measure_callback([=](float d){cout<<ch_i << "," << sig_i << "," <<d<<endl;});
				if (strncmp(sig_info->label, "Voltage", 4) == 0){
					cout << "setting voltage" << endl;
					sig->source_sine(0, 5, 128, 32);
				}
			}
		}
		for (auto i: session->m_devices) {
			session->configure(i->get_default_rate());
		}
		session->start(0);
	};

	while ((c = getopt(argc, argv, "s")) != -1) {
		switch (c) {
			case 's':
				while ( 1 == 1 ) {session->wait_for_completion();};
				break;
			default:
				return 1;
		}
	}
	return 0;
}
