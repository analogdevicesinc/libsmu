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
	Session* session = new Session();
	session->update_available_devices();

	const unsigned count = 200;
	vector<string> names;
	vector<vector<float> > data;

	unsigned dev_i = 0;
	for (auto i: session->m_available_devices) {
		auto dev = session->add_device(&*i);
		auto dev_info = dev->info();

		for (unsigned ch_i=0; ch_i < dev_info->channel_count; ch_i++) {
			auto ch_info = dev->channel_info(ch_i);
			dev->set_mode(ch_i, 1);

			for (unsigned sig_i=0; sig_i < ch_info->signal_count; sig_i++) {
				auto sig = dev->signal(ch_i, sig_i);
				auto sig_info = sig->info();

				names.push_back(std::to_string(dev_i) + "." + string(ch_info->label) + "." + string(sig_info->label));

				data.emplace_back();
				auto buf = data.rbegin();
				buf->resize(count);
				sig->measure_buffer(buf->data(), count);

				if (ch_i == 0) {
					sig->source_sine(1, 1, 10, 0);
				} else {
					sig->source_triangle(2, 3, 10, 0);
				}
			}
		}
		dev_i++;
	}
	for (auto i: session->m_devices) {
		session->configure(i->get_default_rate());
	}

	session->run(count);

	for (auto name: names) {
		cout << name << "\t";
	}
	cout << endl;

	for (unsigned i=0; i<count; i++) {
			for (auto d: data) {
				cout << d[i] << "\t";
			}
			cout << endl;
	}

}
