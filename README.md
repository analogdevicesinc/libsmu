[![Build status](https://ci.appveyor.com/api/projects/status/p30uj8rqulrxsqvs/branch/master?svg=true)](https://ci.appveyor.com/project/analogdevicesinc/libsmu/branch/master)

### libsmu

libsmu contains abstractions for streaming data to and from USB-connected
analog interface devices, currently supporting the Nonolith CEE and Analog
Devices' ADALM1000. Building off of LibUSB for cross-platform operation, it
offers the sourcing of repeated waveforms, configuration of hardware, and
measuring of signals.

This project also includes 'pysmu,' an initial binding of libsmu for Python2.7.

#### Building libsmu

Build dependencies are cmake, pkgconfig, and libusb-1.0. To build and install
the library and command line application use the following commands:

```
git clone https://github.com/analogdevicesinc/libsmu.git
mkdir libsmu/build && cd libsmu/build
cmake ..
make
sudo make install
```

Bindings for python2.7 are also available and are built if enabled via the
following cmake command before running make:

```
cmake -DBUILD_PYTHON=ON ..
```

They can also be built manually via the setup.py script in the regular python
manner if libsmu has already been built and/or installed on the host machine.

##### OS X

For systems running OS X, first install [homebrew](http://brew.sh). Then use
brew to install cmake, libusb, pkg-config, and optionally python (to build the
python bindings):

```
brew install cmake libusb pkg-config python
```

Then the command line instructions in the previous section should work on OS X
as well.

##### Windows

On Windows, it's easier to use the build artifacts provided by Appveyor instead
of rolling your own. Both 32 bit and 64 bit libraries built using mingw are
available:

64 bit:
  - [smu.exe](https://ci.appveyor.com/api/projects/analogdevicesinc/libsmu/artifacts/smu.exe?branch=master&job=Platform%3A%20x64)
  - [libsmu.dll](https://ci.appveyor.com/api/projects/analogdevicesinc/libsmu/artifacts/libsmu.dll?branch=master&job=Platform%3A%20x64)
  - [libpysmu.pyd](https://ci.appveyor.com/api/projects/analogdevicesinc/libsmu/artifacts/libpysmu.pyd?branch=master&job=Platform%3A%20x64)

32 bit:
  - [smu.exe](https://ci.appveyor.com/api/projects/analogdevicesinc/libsmu/artifacts/smu.exe?branch=master&job=Platform%3A%20x86)
  - [libsmu.dll](https://ci.appveyor.com/api/projects/analogdevicesinc/libsmu/artifacts/libsmu.dll?branch=master&job=Platform%3A%20x86)
  - [libpysmu.pyd](https://ci.appveyor.com/api/projects/analogdevicesinc/libsmu/artifacts/libpysmu.pyd?branch=master&job=Platform%3A%20x86)
