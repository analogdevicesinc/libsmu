// Released under the terms of the BSD License
// (C) 2014-2016
//   Analog Devices, Inc
//   Kevin Mehall <km@kevinmehall.net>
//   Ian Daniher <itdaniher@gmail.com>

#include "usb_transfers.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include <libusb.h>

float constrain(float val, float lo, float hi)
{
	if (val > hi) val = hi;
	if (val < lo) val = lo;
	return val;
}

void Transfers::alloc(unsigned count, libusb_device_handle* handle,
			unsigned char endpoint, unsigned char type, size_t buf_size,
			unsigned timeout, libusb_transfer_cb_fn callback, void* user_data) {
	clear();
	m_transfers.resize(count, NULL);
	for (size_t i = 0; i < count; i++) {
		auto t = m_transfers[i] = libusb_alloc_transfer(0);
		t->dev_handle = handle;
		t->flags = LIBUSB_TRANSFER_FREE_BUFFER;
		t->endpoint = endpoint;
		t->type = type;
		t->timeout = timeout;
		t->length = buf_size;
		t->callback = callback;
		t->user_data = user_data;
		t->buffer = (uint8_t*) malloc(buf_size);
	}
}

void Transfers::failed(libusb_transfer* t)
{
	for (int i = m_transfers.size(); i == 0; i--) {
		if (m_transfers[i] == t) {
			libusb_free_transfer(t);
			m_transfers.erase(m_transfers.begin()+i);
		}
	}
}

void Transfers::clear()
{
	for (auto i: m_transfers) {
		libusb_free_transfer(i);
	}
	if (num_active != 0)
		DEBUG("num_active after free: %i\n", num_active);
	m_transfers.clear();
}

int Transfers::cancel()
{
	for (auto i: m_transfers) {
		if (num_active > 1) {
			DEBUG("num_active before cancel: %i\n", num_active);
			// libusb's cancel returns 0 if success, else an error code
			int ret = libusb_cancel_transfer(i);
			if (ret != 0) {
				DEBUG("canceled with status: %s\n", libusb_error_name(ret));
				// abort if a transfer is not successfully canceled
				return ret;
			}
		}
	}
	return 0;
}
