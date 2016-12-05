// Released under the terms of the BSD License
// (C) 2014-2016
//   Analog Devices, Inc
//   Kevin Mehall <km@kevinmehall.net>
//   Ian Daniher <itdaniher@gmail.com>

#include "usb.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <vector>

#include <libusb.h>

#include "debug.hpp"

// Mapping of libusb error codes to system errnos.
static std::map<int, int> libusb_to_errno_map = {
	{LIBUSB_ERROR_INVALID_PARAM, EINVAL},
	{LIBUSB_ERROR_ACCESS, EACCES},
	{LIBUSB_ERROR_NO_DEVICE, ENODEV},
	{LIBUSB_ERROR_NOT_FOUND, ENXIO},
	{LIBUSB_ERROR_BUSY, EBUSY},
	{LIBUSB_ERROR_TIMEOUT, ETIMEDOUT},
	{LIBUSB_ERROR_OVERFLOW, EIO},
	{LIBUSB_ERROR_PIPE, EPIPE},
	{LIBUSB_ERROR_INTERRUPTED, EINTR},
	{LIBUSB_ERROR_NO_MEM, ENOMEM},
	{LIBUSB_ERROR_NOT_SUPPORTED, ENOSYS},
};

unsigned int libusb_to_errno(int libusb_err)
{
	// All libusb errors that require mapping are negative. All non-negative
	// values either define success (0) or relate to a value that should be
	// returned as is.
	if (libusb_err >= 0)
		return libusb_err;

	auto sys_err = libusb_to_errno_map.find(libusb_err);
	if (sys_err != libusb_to_errno_map.end())
		return sys_err->second;
	else
		return EIO;
}

int libusb_errno_or_zero(int ret)
{
	if (ret < 0)
		return -libusb_to_errno(ret);
	else
		return 0;
}

int Transfers::alloc(unsigned count, libusb_device_handle* handle,
			unsigned char endpoint, unsigned char type, size_t buf_size,
			unsigned timeout, libusb_transfer_cb_fn callback, void* user_data) {
	clear();
	m_transfers.resize(count, NULL);
	for (size_t i = 0; i < count; i++) {
		auto t = m_transfers[i] = libusb_alloc_transfer(0);
		if (!t)
			return -ENOMEM;
		t->dev_handle = handle;
		t->flags = LIBUSB_TRANSFER_FREE_BUFFER;
		t->endpoint = endpoint;
		t->type = type;
		t->timeout = timeout;
		t->length = buf_size;
		t->callback = callback;
		t->user_data = user_data;
		t->buffer = (uint8_t*) malloc(buf_size);
		if (!t->buffer)
			return -ENOMEM;
	}
	return 0;
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
	int ret = 0;
	for (auto i: m_transfers) {
		if (num_active > 1) {
			ret = libusb_cancel_transfer(i);
			if (ret != 0) {
				// abort if a transfer is not successfully cancelled
				DEBUG("usb transfer cancelled with status: %s\n", libusb_error_name(ret));
				return -libusb_to_errno(ret);
			}
		}
	}
	return 0;
}
