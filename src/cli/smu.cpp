// Released under the terms of the BSD License
// (C) 2014-2016
//   Analog Devices, Inc.
//   Kevin Mehall <km@kevinmehall.net>
//   Ian Daniher <itdaniher@gmail.com>

#ifdef _WIN32
#include "getopt.h"
#include <io.h>
#else
#include <getopt.h>
#include <unistd.h>
#endif

#include <cstdint>
#include <cstring>
#include <chrono>
#include <iostream>
#include <vector>
#include <system_error>
#include <thread>

#include <libsmu/libsmu.hpp>

#ifndef _WIN32
int (*_isatty)(int) = &isatty;
#endif

using std::cout;
using std::cerr;
using std::endl;

using namespace smu;

static void list_devices(Session* session)
{
	if (session->m_devices.empty()) {
		cerr << "smu: no supported devices plugged in" << endl;
	} else {
		for (auto dev: session->m_devices) {
			printf("%s: serial %s: fw %s: hw %s\n",
					dev->info()->label, dev->m_serial.c_str(),
					dev->m_fwver.c_str(), dev->m_hwver.c_str());
		}
	}
}

static void display_usage(void)
{
	printf("smu: utility for managing M1K devices\n"
		"\n"
		" -h, --help                   print this help message and exit\n"
		" --version                    show libsmu version\n"
		" -l, --list-devices           list supported devices currently attached to the system\n"
		" -p, --hotplug-devices        simple session device hotplug testing\n"
		" -s, --stream-samples         stream samples to stdout from a single attached device\n"
		" -d, --display-calibration    display calibration data from all attached devices\n"
		" -r, --reset-calibration      reset calibration data to the defaults on all attached devices\n"
		" -w, --write-calibration <cal file> write calibration data to a single attached device\n"
		" -f, --flash <firmware image> flash firmware image to a single attached device\n");
}

static void stream_samples(Session* session)
{
	auto dev = *(session->m_devices.begin());
	auto dev_info = dev->info();
	for (unsigned ch_i = 0; ch_i < dev_info->channel_count; ch_i++) {
		dev->set_mode(ch_i, HI_Z);
	}

	session->hotplug_detach([=](Device* device, void* data){
		throw std::runtime_error("device detached");
	});

	session->configure(dev->get_default_rate());
	session->start(0);
	std::vector<std::array<float, 4>> buf;
	unsigned dev_index;

	while (true) {
		dev_index = 0;
		for (auto dev: session->m_devices) {
			try {
				dev->read(buf, 1000, -1);
			} catch (const std::runtime_error& e) {
				// Only error out if stdout isn't a tty, otherwise it usually
				// can't keep up anyway.
				if (!_isatty(1)) {
					cerr << "smu: stopping stream: " << e.what() << endl;
					return;
				}
			}

			for (auto x: buf) {
				printf("dev %u: %f %f %f %f\n", dev_index, x[0], x[1], x[2], x[3]);
			}
			dev_index++;
		}
	};
}

int write_calibration(Session* session, const char *file)
{
	int ret;
	auto dev = *(session->m_devices.begin());
	ret = dev->write_calibration(file);
	if (ret < 0) {
		if (ret == -EINVAL)
			cerr << "smu: invalid calibration data format" << endl;
		else if (ret == -EPIPE)
			cerr << "smu: firmware version doesn't support calibration (update to 2.06 or later)" << endl;
		else
			perror("smu: failed to write calibration data");
		return 1;
	}
	return 0;
}

void display_calibration(Session* session)
{
	std::vector<std::vector<float>> cal;
	for (auto dev: session->m_devices) {
		printf("%s: serial %s: fw %s: hw %s\n",
			dev->info()->label, dev->m_serial.c_str(),
			dev->m_fwver.c_str(), dev->m_hwver.c_str());
		dev->calibration(&cal);
		for (int i = 0; i < 8; i++) {
			switch (i) {
				case 0: printf("  Channel A, measure V\n"); break;
				case 1: printf("  Channel A, measure I\n"); break;
				case 2: printf("  Channel A, source V\n"); break;
				case 3: printf("  Channel A, source I\n"); break;
				case 4: printf("  Channel B, measure V\n"); break;
				case 5: printf("  Channel B, measure I\n"); break;
				case 6: printf("  Channel B, source V\n"); break;
				case 7: printf("  Channel B, source I\n"); break;
			}
			printf("    offset: %.4f\n", cal[i][0]);
			printf("    p gain: %.4f\n", cal[i][1]);
			printf("    n gain: %.4f\n", cal[i][2]);
		}
		printf("\n");
	}
}

int reset_calibration(Session* session)
{
	int ret;
	for (auto dev: session->m_devices) {
		ret = dev->write_calibration(NULL);
		if (ret < 0) {
			if (ret == -EPIPE)
				cerr << "smu: firmware version doesn't support calibration (update to 2.06 or later)" << endl;
			else
				perror("smu: failed to reset calibration data");
			return 1;
		}
	}
	return 0;
}

int main(int argc, char **argv)
{
	int opt;
	int option_index = 0;

	// display usage info if no arguments are specified
	if (argc == 1) {
		display_usage();
		return EXIT_FAILURE;
	}

	Session* session = new Session();
	// add all available devices to the session at startup
	if (session->add_all() < 0) {
		perror("smu: error initializing session");
		return 1;
	}

	// map long options to short ones
	static struct option long_options[] = {
		{"help",     no_argument, 0, 'a'},
		{"version",     no_argument, 0, 'v'},
		{"hotplug",  no_argument, 0, 'p'},
		{"list",     no_argument, 0, 'l'},
		{"stream",   no_argument, 0, 's'},
		{"display-calibration", no_argument, 0, 'd'},
		{"reset-calibration", no_argument, 0, 'r'},
		{"write-calibration", required_argument, 0, 'w'},
		{"flash", required_argument, 0, 'f'},
		{0, 0, 0, 0}
	};

	while ((opt = getopt_long(argc, argv, "hvplsdrw:f:",
			long_options, &option_index)) != -1) {
		switch (opt) {
			case 'p':
				session->hotplug_detach([=](Device* device, void* data){
					session->cancel();
					if (!session->remove(device, true)) {
						printf("removed device: %s: serial %s: fw %s: hw %s\n",
							device->info()->label, device->m_serial.c_str(),
							device->m_fwver.c_str(), device->m_hwver.c_str());
					}
				});

				session->hotplug_attach([=](Device* device, void* data){
					if (!session->add(device)) {
						printf("added device: %s: serial %s: fw %s: hw %s\n",
							device->info()->label, device->m_serial.c_str(),
							device->m_fwver.c_str(), device->m_hwver.c_str());
					}
				});

				cout << "waiting for hotplug events..." << endl;

				// wait around doing nothing (hotplug testing)
				while (1)
					std::this_thread::sleep_for(std::chrono::seconds(10));
				break;
			case 'l':
				// list attached device info
				list_devices(session);
				break;
			case 's':
				// stream samples from an attached device to stdout
				if (!session->m_devices.empty()) {
					stream_samples(session);
				} else {
					cerr << "smu: no supported devices plugged in" << endl;
					return EXIT_FAILURE;
				}
				break;
			case 'd':
				// display calibration data from all attached m1k devices
				display_calibration(session);
				break;
			case 'r':
				// reset calibration data of all attached m1k devices
				if (session->m_devices.empty()) {
					cerr << "smu: no supported devices plugged in" << endl;
					return EXIT_FAILURE;
				}
				if (reset_calibration(session)) {
					return EXIT_FAILURE;
				}
				cout << "smu: successfully reset calibration data" << endl;
				break;
			case 'w':
				// write calibration data to a single attached m1k device
				if (session->m_devices.empty()) {
					cerr << "smu: no supported devices plugged in" << endl;
					return EXIT_FAILURE;
				} else if (session->m_devices.size() > 1) {
					cerr << "smu: multiple devices attached, calibration only works on a single device" << endl;
					cerr << "Please detach all devices except the one targeted for calibration." << endl;
					return EXIT_FAILURE;
				}
				if (write_calibration(session, optarg))
					return EXIT_FAILURE;
				cout << "smu: successfully updated calibration data" << endl;
				break;
			case 'f':
				// flash firmware image to an attached m1k device
				try {
					session->flash_firmware(optarg);
				} catch (const std::exception& e) {
					cout << "smu: failed updating firmware: " << e.what() << endl;
					return EXIT_FAILURE;
				}
				cout << "smu: successfully updated firmware" << endl;
				cout << "Please unplug and replug the device to finish the process." << endl;
				break;
			case 'h':
				display_usage();
				break;
			case 'v':
				cout << LIBSMU_VERSION_STR << endl;
				break;
			default:
				display_usage();
				return EXIT_FAILURE;
		}
	}

	delete session;
	return EXIT_SUCCESS;
}
