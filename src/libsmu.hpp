// Released under the terms of the BSD License
// (C) 2014-2016
//   Analog Devices, Inc.
//   Kevin Mehall <km@kevinmehall.net>
//   Ian Daniher <itdaniher@gmail.com>

#pragma once

#include "version.hpp"
#include "device.hpp"
#include "signal.hpp"
#include "session.hpp"

#include <cstdint>
#include <vector>

/// List of supported devices, using vendor and project IDs from USB
/// information formatted as {vendor_id, product_id}.
const std::vector<std::vector<uint16_t>> SUPPORTED_DEVICES = {
	{0x0456, 0xcee2},
	{0x064b, 0x784c},
};

/// List of supported devices in SAM-BA bootloader mode, using vendor and
/// project IDs from USB information formatted as {vendor_id, product_id}.
const std::vector<std::vector<uint16_t>> SAMBA_DEVICES = {
	{0x03eb, 0x6124},
};
