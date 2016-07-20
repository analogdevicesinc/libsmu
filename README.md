[![appveyor status](https://ci.appveyor.com/api/projects/status/p30uj8rqulrxsqvs/branch/master?svg=true)](https://ci.appveyor.com/project/analogdevicesinc/libsmu/branch/master)
[![travis-ci status](https://travis-ci.org/analogdevicesinc/libsmu.svg?branch=master)](https://travis-ci.org/analogdevicesinc/libsmu)

### libsmu

libsmu contains abstractions for streaming data to and from USB-connected
analog interface devices, currently supporting the Analog Devices' ADALM1000.
Building off of LibUSB for cross-platform operation, it offers the sourcing of
repeated waveforms, configuration of hardware, and measuring of signals.

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

##### Linux

By default, libsmu is installed into various directories inside /usr/local. If
run-time linking errors occur, try running `sudo ldconfig` to regenerate the
run-time linker cache.

If the same errors still happen, make sure the directory the libsmu library is
installed to is in the sourced files for /etc/ld.so.conf before running
ldconfig.

##### OS X

For systems running OS X, first install [homebrew](http://brew.sh). Then use
brew to install libusb, cmake, pkg-config, and optionally python (to build the
python bindings):

```
brew install libusb --universal
brew install cmake pkg-config python
```

Then the command line instructions in the previous section should work on OS X
as well.

##### Windows

On Windows, it's easiest to use the provided installers,
[libsmu-setup-x86.exe](https://ci.appveyor.com/api/projects/analogdevicesinc/libsmu/artifacts/libsmu-setup-x86.exe?branch=master&job=Configuration%3A%20Release) and
[libsmu-setup-x64.exe](https://ci.appveyor.com/api/projects/analogdevicesinc/libsmu/artifacts/libsmu-setup-x64.exe?branch=master&job=Configuration%3A%20Release)
that install either 32 or 64 bit support, respectively. During the
install process options are provided to install python and Visual Studio
development support.
