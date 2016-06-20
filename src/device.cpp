// Released under the terms of the BSD License
// (C) 2014-2016
//   Analog Devices, Inc.
//   Kevin Mehall <km@kevinmehall.net>
//   Ian Daniher <itdaniher@gmail.com>

#include "debug.hpp"
#include <libsmu/device.hpp>
#include <libsmu/session.hpp>

#include <libusb.h>

using namespace smu;

Device::Device(Session* s, libusb_device* d): m_session(s), m_device(d)
{
	libusb_ref_device(m_device);
}

int Device::init()
{
	int r = libusb_open(m_device, &m_usb);
	return r;
}

Device::~Device()
{
	if (m_usb)
		libusb_close(m_usb);
	if (m_device)
		libusb_unref_device(m_device);
}

int Device::ctrl_transfer(unsigned bmRequestType, unsigned bRequest, unsigned wValue, unsigned wIndex, unsigned char *data, unsigned wLength, unsigned timeout)
{
	return libusb_control_transfer(m_usb, bmRequestType, bRequest, wValue, wIndex, data, wLength, timeout);
}
