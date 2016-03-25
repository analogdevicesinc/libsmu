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

On Windows, it's easiest to use the provided installer
([libsmu-setup.exe](https://ci.appveyor.com/api/projects/analogdevicesinc/libsmu/artifacts/libsmu-setup.exe?branch=master))
that installs either 32 or 64 bit support based off the host system.

Prebuilt python support is also available and requires libsmu to be installed
in addition to python-2.7:

- 32 bit: [pysmu-0.88.win32-py2.7.msi](https://ci.appveyor.com/api/projects/analogdevicesinc/libsmu/artifacts/pysmu-0.88.win32-py2.7.msi?branch=master)
- 64 bit: [pysmu-0.88.win-amd64-py2.7.msi](https://ci.appveyor.com/api/projects/analogdevicesinc/libsmu/artifacts/pysmu-0.88.win-amd64-py2.7.msi?branch=master)
