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

// Map libusb error codes to system errnos.
// If there is no match, EIO is returned.
unsigned int libusb_to_errno(int libusb_err);

// Map libusb error codes to negative system errnos and positive return values
// (relating to the byte count of successful calls) to zero.
int libusb_errno_or_zero(int libusb_err);

// Wrapper for a collection of libusb transfers.
class Transfers {
	public:
		~Transfers() { clear(); }

		// Currently running usb tranfers.
		std::vector<libusb_transfer*> m_transfers;

		// Allocate a new collection of libusb transfers.
		// @return 0 if transfer allocation successful.
		// @return 1 if transfer allocation failed.
		int alloc(unsigned count, libusb_device_handle* handle,
				unsigned char endpoint, unsigned char type, size_t buf_size,
				unsigned timeout, libusb_transfer_cb_fn callback, void* user_data);

		// Remove a transfer that was not successfully submitted from the
		// collection of pending transfers.
		void failed(libusb_transfer* t);

		// Free and clear collection of libusb transfers.
		void clear();

		// Signal cleanup - stop streaming and cleanup libusb state.
		// Loop over pending transfers, canceling each remaining transfer that
		// hasn't already been canceled. Returns an error code if one of the
		// transfers doesn't complete, or zero for success.
		int cancel();

		// Number of current usb transfers.
		size_t size() { return m_transfers.size(); }

		// Treat m_transfers like an iterator.
		typedef std::vector<libusb_transfer*>::iterator iterator;
		typedef std::vector<libusb_transfer*>::const_iterator const_iterator;
		iterator begin() { return m_transfers.begin(); }
		const_iterator begin() const { return m_transfers.begin(); }
		iterator end() { return m_transfers.end(); }
		const_iterator end() const { return m_transfers.end(); }

		// Current number of pending transfers.
		int32_t num_active = 0;
};
