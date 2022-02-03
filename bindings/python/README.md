# libsmu : Python bindings

This package contains the python bindings for libsmu.
libsmu contains abstractions for streaming data to and from USB-connected analog interface devices, currently supporting the Analog Devices' ADALM1000. Building off of LibUSB for cross-platform operation, it offers the sourcing of repeated waveforms, configuration of hardware, and measuring of signals.

[![Build Status (Linux, Windows, MacOS)](https://dev.azure.com/AnalogDevices/Libsmu/_apis/build/status/analogdevicesinc.libsmu?branchName=master)](https://dev.azure.com/AnalogDevices/Libsmu/_build/latest?definitionId=32&branchName=master)

[[Docs](http://analogdevicesinc.github.io/libsmu/)]
[[Support](http://ez.analog.com)]
[[Wiki](https://wiki.analog.com/university/tools/m1k/libsmu)]

## Requirements
To use these bindings you need the core C++ library they depend upon. This is not packaged with the pypi release but you can install the [latest release](https://github.com/analogdevicesinc/libsmu/releases/latest) or the latest **untested** binaries from the [master branch](https://ci.appveyor.com/project/analogdevicesinc/libsmu).

### Installing
You can install these bindings using pip, if you already have the library installed:
```shell
(sudo) pip install libsmu
```

If you want to build them manually, please check the [build guide](https://wiki.analog.com/university/tools/m1k/libsmu#how_to_build_it) for your specific operating system.
