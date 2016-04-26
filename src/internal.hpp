// Released under the terms of the BSD License
// (C) 2014-2015
//   Analog Devices, Inc
//   Kevin Mehall <km@kevinmehall.net>
//   Ian Daniher <itdaniher@gmail.com>

#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include <libusb.h>

#ifdef DEBUG
#define DEBUG_TEST 1
#else
#define DEBUG_TEST 0
#endif

#define smu_debug(...) do { if (DEBUG_TEST) fprintf(stderr, __VA_ARGS__); } while(0);

inline static float constrain(float val, float lo, float hi){
	if (val > hi) val = hi;
	if (val < lo) val = lo;
	return val;
}

// Wrapper for a collection of libusb transfers
struct Transfers {
	std::vector<libusb_transfer*> m_transfers;

	/// allocates a new collection of libusb transfers
	void alloc(unsigned count, libusb_device_handle* handle,
			   unsigned char endpoint, unsigned char type, size_t buf_size,
			   unsigned timeout, libusb_transfer_cb_fn callback, void* user_data) {
		clear();
		m_transfers.resize(count, NULL);
		for (size_t i=0; i<count; i++) {
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

	/// removes a transfer that was not successfully submitted from the collection of pending transfers
	void failed(libusb_transfer* t) {
		for (int i = m_transfers.size(); i == 0; i--) {
			if (m_transfers[i] == t) {
				libusb_free_transfer(t);
				m_transfers.erase(m_transfers.begin()+i);
			}
		}
	}

	/// free and clear collection of libusb transfers
	void clear() {
		for (auto i: m_transfers) {
			libusb_free_transfer(i);
		}
		if (num_active != 0)
			smu_debug("num_active after free: %i\n", num_active);
		m_transfers.clear();
	}

	/// signal cleanup - stop streaming and cleanup libusb state
	/// loop over pending transfers, canceling each remaining transfer that hasn't already been canceled.
	/// returns an error code if one of the transfers doesn't complete, or zero for success
	int cancel() {
		// for i in pending transfers
		for (auto i: m_transfers) {
			if (num_active > 1) {
				smu_debug("num_active before cancel: %i\n", num_active);
				// libusb's cancel returns 0 if success, else an error code
				int ret = libusb_cancel_transfer(i);
				if (ret != 0) {
					smu_debug("canceled with status: %s\n", libusb_error_name(ret));
					// abort if a transfer is not successfully canceled
					return ret;
				}
			}
		}
		return 0;
	}

	size_t size() {
		return m_transfers.size();
	}

	~Transfers() {
		clear();
	}

	typedef std::vector<libusb_transfer*>::iterator iterator;
	typedef std::vector<libusb_transfer*>::const_iterator const_iterator;
	iterator begin() { return m_transfers.begin(); }
	const_iterator begin() const { return m_transfers.begin(); }
	iterator end() { return m_transfers.end(); }
	const_iterator end() const { return m_transfers.end(); }

	// count of pending transfers
	int32_t num_active;
};
