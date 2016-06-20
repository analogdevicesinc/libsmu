// Released under the terms of the BSD License
// (C) 2014-2016
//   Analog Devices, Inc.
//   Kevin Mehall <km@kevinmehall.net>
//   Ian Daniher <itdaniher@gmail.com>

/// @file libsmu.hpp
/// @brief Main public interface.
/// This is the main header that should generally be the only one
/// imported by third parties. All public classes and methods will be
/// available through it.

#pragma once

#include <libsmu/version.hpp>
#include <libsmu/device.hpp>
#include <libsmu/signal.hpp>
#include <libsmu/session.hpp>

#include <cstdint>
#include <vector>

/// @brief List of supported devices.
/// The list uses the vendor and project IDs from USB
/// information formatted as {vendor_id, product_id}.
const std::vector<std::vector<uint16_t>> SUPPORTED_DEVICES = {
	{0x0456, 0xcee2},
	{0x064b, 0x784c},
};

/// @brief List of supported devices in SAM-BA bootloader mode.
/// The list uses the vendor and project IDs from USB information
/// formatted as {vendor_id, product_id}.
const std::vector<std::vector<uint16_t>> SAMBA_DEVICES = {
	{0x03eb, 0x6124},
};
