#include <memory>
#include "libsmu.hpp"
#include <iostream>

using std::cout;
using std::cerr;
using std::endl;
using std::unique_ptr;

int main() {
	unique_ptr<Session> session = unique_ptr<Session>(new Session());
	session->update_available_devices();

	for (auto i: session->m_available_devices) {
		cout << "Device" << endl;
		session->add_device(&*i);
	}
}
