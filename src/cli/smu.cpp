// Released under the terms of the BSD License
// (C) 2014-2016
//   Analog Devices, Inc.
//   Kevin Mehall <km@kevinmehall.net>
//   Ian Daniher <itdaniher@gmail.com>

#include "libsmu.hpp"
#include <iostream>
#include <cstdint>
#include <vector>
#include <thread>
#include <string.h>

#ifdef WIN32
#include "getopt.h"
#else
#include <getopt.h>
#endif

using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::vector;

static void list_devices(Session* session)
{
	for (auto dev: session->m_devices) {
		printf("%s: serial %s: fw %s: hw %s\n",
				dev->info()->label, dev->serial(),
				dev->fwver(), dev->hwver());
	}
}

static void display_usage(void)
{
	printf("smu: simple libsmu-based tool\n"
		"\n"
		"  -h, --help                   print this help message and exit\n"
		"  -l, --list                   list supported devices currently attached to the system\n"
		"  -p, --hotplug                simple session device hotplug testing\n"
		"  -s, --stream                 stream samples to stdout from a single attached device\n"
		"  -c, --calibrate <cal file>   write calibration data to a single attached device\n"
		"  -d, --display-calibration    display calibration data from all attached devices\n"
		"  -f, --flash <firmware image> flash firmware image to all attached devices\n");
}

static void stream_samples(Session* session)
{
	auto dev = *(session->m_devices.begin());
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
	session->configure(dev->get_default_rate());
	session->start(0);
	while ( 1 == 1 ) {session->wait_for_completion();};
}

int calibrate(Session* session, const char *file)
{
	int ret;
	auto dev = *(session->m_devices.begin());
	if (strncmp(dev->info()->label, "ADALM1000", 9) == 0) {
		ret = dev->write_calibration(file);
		if (ret <= 0) {
			if (ret == -EINVAL)
				cerr << "smu: invalid calibration data, overwritten using defaults" << endl;
			else
				perror("smu: failed to write calibration data");
			return 1;
		}
	} else {
		cerr << "smu: calibration only works with ADALM1000 devices" << endl;
	}
	return 0;
}

void display_calibration(Session* session)
{
	vector<vector<float>> cal;
	for (auto dev: session->m_devices) {
		if (strncmp(dev->info()->label, "ADALM1000", 9) == 0) {
			printf("%s: serial %s: fw %s: hw %s\n",
				dev->info()->label, dev->serial(),
				dev->fwver(), dev->hwver());
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
}

int flash_firmware(Session* session, const char *file)
{
	for (auto dev: session->m_devices) {
		if (strncmp(dev->info()->label, "ADALM1000", 9) == 0) {
			if (dev->flash_firmware(file)) {
				perror("smu: failed to flash firmware image");
				return 1;
			}
		}
	}
	return 0;
}

int main(int argc, char **argv)
{
	int opt, ret = 0;
	int option_index = 0;

	Session* session = new Session();
	// add all available devices to the session at startup
	if (session->update_available_devices()) {
		cerr << "error initializing session" << endl;
		return 1;
	}
	for (auto dev: session->m_available_devices) {
		session->add_device(&*dev);
	}

	session->m_completion_callback = [=](unsigned status){};
	session->m_progress_callback = [=](sample_t n){};

	session->m_hotplug_detach_callback = [=](Device* device){
		session->cancel();
		session->remove_device(device);
		printf("removed device: %s: serial %s: fw %s: hw %s\n",
				device->info()->label, device->serial(),
				device->fwver(), device->hwver());
	};

	session->m_hotplug_attach_callback = [=](Device* device){
		if (session->add_device(device))
			printf("added device: %s: serial %s: fw %s: hw %s\n",
				device->info()->label, device->serial(),
				device->fwver(), device->hwver());
	};

	// map long options to short ones
	static struct option long_options[] = {
		{"help",     no_argument, 0, 'a'},
		{"hotplug",  no_argument, 0, 'p'},
		{"list",     no_argument, 0, 'l'},
		{"stream",   no_argument, 0, 's'},
		{"display-calibration", no_argument, 0, 'd'},
		{"calibrate", required_argument, 0, 'c'},
		{"flash", required_argument, 0, 'f'},
		{0, 0, 0, 0}
	};

	while ((opt = getopt_long(argc, argv, "hplsdc:f:",
			long_options, &option_index)) != -1) {
		switch (opt) {
			case 'p':
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
					exit(EXIT_FAILURE);
				}
				break;
			case 'd':
				// display calibration data from all attached m1k devices
				display_calibration(session);
				break;
			case 'c':
				// write calibration data to a single attached m1k device
				if (session->m_devices.empty()) {
					cerr << "smu: no supported devices plugged in" << endl;
					exit(EXIT_FAILURE);
				} else if (session->m_devices.size() > 1) {
					cerr << "smu: multiple devices attached, calibration only works on a single device" << endl;
					cerr << "Please detach all devices except the one targeted for calibration." << endl;
					exit(EXIT_FAILURE);
				}
				if (calibrate(session, optarg))
					exit(EXIT_FAILURE);
				break;
			case 'f':
				// flash firmware image to all attached m1k devices
				if (flash_firmware(session, optarg))
					exit(EXIT_FAILURE);
				break;
			case 'h':
				display_usage();
				break;
			default:
				display_usage();
				exit(EXIT_FAILURE);
		}
	}

	// default to listing attached devices if no option is specified
	if (optind >= argc) {
		list_devices(session);
	}

	exit(EXIT_SUCCESS);
}
