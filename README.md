[![Build status](https://ci.appveyor.com/api/projects/status/p30uj8rqulrxsqvs/branch/master?svg=true)](https://ci.appveyor.com/project/analogdevicesinc/libsmu/branch/master)

### LibSMU

LibSMU contains abstractions for streaming data to and from USB-connected
analog interface devices, currently supporting the Nonolith CEE and Analog
Devices' ADALM1000. Building off of LibUSB for cross-platform operation, it
offers the sourcing of repeated waveforms, configuration of hardware, and
measuring of signals.

This project also includes 'pysmu,' an initial binding of LibSMU for Python2.7.

#### Building LibSMU

With libusb-1.0, and clang++, building LibSMU and the demo 'smu' executable is
as simple as typing `make`.

With python2.7 and associated development headers installed, building pysmu.so,
importable as `import pysmu` is as simple as typing `make python`.

##### Notes on Mac OS X

First install [homebrew](http://brew.sh). Then use brew to install libusb, pkg-config, and python:

```
brew install libusb pkg-config python
```

Finally build libSMU:

```
git clone https://github.com/analogdevicesinc/libsmu.git
cd libsmu
make
```

##### Notes on Windows

On Windows, it's easier to use the builds provided by Appveyor instead of
building your own. Both 32 bit and 64 bit builds built using mingw are
available:

64 bit:

[smu.exe](https://ci.appveyor.com/api/projects/analogdevicesinc/libsmu/artifacts/bin/smu.exe?branch=master&job=Platform%3A%20x64)
[libsmu.dll](https://ci.appveyor.com/api/projects/analogdevicesinc/libsmu/artifacts/bin/libsmu.dll?branch=master&job=Platform%3A%20x64)
[libsmu.pyd](https://ci.appveyor.com/api/projects/analogdevicesinc/libsmu/artifacts/bin/libsmu.pyd?branch=master&job=Platform%3A%20x64)

32 bit:

[smu.exe](https://ci.appveyor.com/api/projects/analogdevicesinc/libsmu/artifacts/bin/debug.zip?branch=master&job=Platform%3A%20x86)
[libsmu.dll](https://ci.appveyor.com/api/projects/analogdevicesinc/libsmu/artifacts/bin/debug.zip?branch=master&job=Platform%3A%20x86)
[libsmu.pyd](https://ci.appveyor.com/api/projects/analogdevicesinc/libsmu/artifacts/bin/debug.zip?branch=master&job=Platform%3A%20x86)
