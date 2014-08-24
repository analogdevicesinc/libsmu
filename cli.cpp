#include <memory>
#include "libsmu.hpp"
#include <iostream>
#include <vector>
#include <cstdint>
#include <string>

using std::cout;
using std::cerr;
using std::endl;
using std::unique_ptr;
using std::vector;
using std::string;

int main() {
	unique_ptr<Session> session = unique_ptr<Session>(new Session());
	session->update_available_devices();

	const unsigned count = 10000;
	vector<string> names;

	unsigned dev_i = 0;
	for (auto i: session->m_available_devices) {
		auto dev = session->add_device(&*i);
		auto dev_info = dev->info();

		for (int ch_i=0; ch_i < dev_info->channel_count; ch_i++) {
			auto ch_info = dev->channel_info(ch_i);
			for (int sig_i=0; sig_i < ch_info->signal_count; sig_i++) {
				auto sig = dev->signal(ch_i, sig_i);
				auto sig_info = sig->info();

				names.push_back(std::to_string(dev_i) + "." + string(ch_info->label) + "." + string(sig_info->label));
			}
		}
		dev_i++;
	}

	session->configure(count);
	session->run(count);

	for (auto name: names) {
		cout << name << "\t";
	}
	cout << endl;
}
