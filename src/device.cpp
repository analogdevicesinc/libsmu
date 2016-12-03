// Released under the terms of the BSD License
// (C) 2014-2016
//   Analog Devices, Inc.
//   Kevin Mehall <km@kevinmehall.net>
//   Ian Daniher <itdaniher@gmail.com>

#include <libusb.h>

#include "debug.hpp"

#include <libsmu/libsmu.hpp>

using namespace smu;

Device::Device(Session* s, libusb_device* d, libusb_device_handle *h,
	const char* hwver, const char* fwver, const char* serial):
	m_hwver(hwver), m_fwver(fwver), m_serial(serial), m_session(s), m_device(d), m_usb(h)
{
	libusb_ref_device(m_device);
}

Device::~Device()
{
	if (m_usb) {
		libusb_release_interface(m_usb, 0);
		libusb_close(m_usb);
	}
	if (m_device)
		libusb_unref_device(m_device);
}

int Device::ctrl_transfer(unsigned bmRequestType, unsigned bRequest, unsigned wValue, unsigned wIndex, unsigned char *data, unsigned wLength, unsigned timeout)
{
	return libusb_control_transfer(m_usb, bmRequestType, bRequest, wValue, wIndex, data, wLength, timeout);
}
