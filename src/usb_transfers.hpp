// Released under the terms of the BSD License
// (C) 2014-2016
//   Analog Devices, Inc
//   Kevin Mehall <km@kevinmehall.net>
//   Ian Daniher <itdaniher@gmail.com>

#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include <libusb.h>

#include "debug.hpp"

float constrain(float val, float lo, float hi);

// Wrapper for a collection of libusb transfers.
class Transfers {
	public:
		std::vector<libusb_transfer*> m_transfers;

		// allocates a new collection of libusb transfers
		void alloc(unsigned count, libusb_device_handle* handle,
				unsigned char endpoint, unsigned char type, size_t buf_size,
				unsigned timeout, libusb_transfer_cb_fn callback, void* user_data);

		// Removes a transfer that was not successfully submitted from the
		// collection of pending transfers.
		void failed(libusb_transfer* t);

		// Free and clear collection of libusb transfers.
		void clear();

		// Signal cleanup - stop streaming and cleanup libusb state.
		// Loop over pending transfers, canceling each remaining transfer that
		// hasn't already been canceled. Returns an error code if one of the
		// transfers doesn't complete, or zero for success
		int cancel();

		size_t size() { return m_transfers.size(); }

		~Transfers() { clear(); } 

		typedef std::vector<libusb_transfer*>::iterator iterator;
		typedef std::vector<libusb_transfer*>::const_iterator const_iterator;
		iterator begin() { return m_transfers.begin(); }
		const_iterator begin() const { return m_transfers.begin(); }
		iterator end() { return m_transfers.end(); }
		const_iterator end() const { return m_transfers.end(); }

		// Count of pending transfers
		int32_t num_active;
};
