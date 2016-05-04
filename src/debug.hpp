// Released under the terms of the BSD License
// (C) 2014-2016, Analog Devices, Inc

#pragma once

#include <cstdio>

#ifdef DEBUG
#define DEBUG_TEST 1
#else
#define DEBUG_TEST 0
#endif

#define smu_debug(...) do { if (DEBUG_TEST) fprintf(stderr, __VA_ARGS__); } while(0);
