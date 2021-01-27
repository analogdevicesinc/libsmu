[![appveyor status](https://ci.appveyor.com/api/projects/status/p30uj8rqulrxsqvs/branch/master?svg=true)](https://ci.appveyor.com/project/analogdevicesinc/libsmu/branch/master)
[![travis-ci status](https://travis-ci.org/analogdevicesinc/libsmu.svg?branch=master)](https://travis-ci.org/analogdevicesinc/libsmu)
[![coverity status](https://scan.coverity.com/projects/analogdevicesinc-libsmu/badge.svg)](https://scan.coverity.com/projects/analogdevicesinc-libsmu)

# libsmu

libsmu contains abstractions for streaming data to and from USB-connected
analog interface devices, currently supporting the Analog Devices' ADALM1000.
Building off of LibUSB for cross-platform operation, it offers the sourcing of
repeated waveforms, configuration of hardware, and measuring of signals.

Python bindings are also provided in the form of the pysmu module. See
instructions below for how to build them.

# Simple installation
## Conda packages
Conda and Anaconda are cross-platform package-management tools that generally focus around python but can support any language or package generally.
You can find documentation on the libsmu conda package [here](https://wiki.analog.com/university/tools/conda?s[]=libsmu) .

## Linux
Download the specific libsmu .deb package for your Linux distribution from the Releases section. Currently we are supporting Ubuntu 16, 18 and 20. The package name should start with libsmu and contain the OS version.
Go to the folder you downloaded the package in and open a terminal, then run the following command:
```shell
analog@analog:~$ sudo apt install -f ./<libsmu_package_name>.deb
```

## Python bindings for Linux
If you want to install pysmu (the Python bindings for libsmu), you can download the specific .deb package. Currently we are supporting the default Python versions for each Ubuntu version (3.5 for Ubuntu 16, 3.6 for Ubuntu 18, 3.8 for Ubuntu 20). The package name should start with python3 and contain the OS version. These packages contain the Python bindings for libsmu.
Go to the folder you downloaded the package in and open a terminal, then run the following command:
```shell
analog@analog:~$ sudo apt install -f ./<python_package_name>.deb
```

## MacOS
Download the specific libsmu .pkg package for your MacOS distribution from the Releases section. Currently we are supporting MacOS 10.13, 10.14 and 10.15. The package name should start with libsmu and contain the MacOS version.

Open a terminal and run the following command which will install only the base library in /Library/Frameworks.
```shell
analog@analog:~$ sudo installer -pkg /path/to/<libsmu_package_name>.pkg -target /
```

A different way to install libsmu on MacOS is by using the .tar.gz located in the Releases section. This will install the .dylib (libraries) into system paths (usr/local) and it will also install the smu CLI.
```shell
tar -xzvf <libsmu_package_name>.tar.gz --strip=3 -C /usr/local
```
Based on this base library installation, you can install the Python bindings manually for the desired Python version. Check the **Python** section below.


# Build instructions for libsmu on Linux
## Install Dependencies
Install prerequisites
```shell
analog@analog:~$ sudo apt-get update
analog@analog:~$ sudo apt-get install libusb-1.0-0-dev libboost-all-dev cmake pkg-config
```
Install to build Python bindings
```shell
analog@analog:~$ sudo apt-get install python3 python3-setuptools python3-pip
analog@analog:~$ pip3 install --upgrade pip
analog@analog:~$ sudo pip3 install cython
```
Install to build documentation
```shell
analog@analog:~$ sudo apt-get install doxygen
```

# Build instructions for libsmu on MacOS
## Install Dependencies
Install prerequisites
```shell
analog@analog:~$ brew update
analog@analog:~$ brew install libusb cmake pkg-config boost
(optional) analog@analog:~$ brew link --overwrite boost
```
Install to build Python bindings
```shell
analog@analog:~$ brew install python3
analog@analog:~$ pip3 install --upgrade pip
analog@analog:~$ pip3 install cython
```
Setup tools and pip should be included in the "python3" package.

Install to build documentation
```shell
analog@analog:~$ brew install doxygen
```

# Clone, configure and build
```shell
analog@analog:~$ git clone https://github.com/analogdevicesinc/libsmu.git
analog@analog:~$ cd libsmu 
```
Options:
CMake Options       | Default | Description                                    |
------------------- | ------- | ---------------------------------------------- |
`BUILD_CLI`   | ON | Build command line smu application                                |
`BUILD_PYTHON`   | ON | Build python bindings                            |
`WITH_DOC`          | OFF | Generate documentation with Doxygen and Sphinx     |
`BUILD_EXAMPLES`        |  OFF | Build examples                            |
`USE_PYTHON2`  | ON | By default, CMake will search for Python 2 or 3. If USE_PYTHON2 is set to OFF, then only Python3 will be used.        |
`INSTALL_UDEV_RULES` |  ON | Install a udev rule for detection of USB devices   |

Configure via cmake:
```shell
analog@analog:~$ mkdir build && cd build
analog@analog:~$ cmake .. -DBUILD_PYTHON=ON
```

Compile:
```shell
analog@analog:~$ make
```

# Install
If **-DBUILD_PYTHON=ON** (from the above options) is specified, this step will also install the Python Bindings using the Python version detected at the CMake configuration step.
```shell
analog@analog:~$ sudo make install
```

# Docs

Doxygen-based documentation is available at
https://analogdevicesinc.github.io/libsmu/.

This can also be built locally if enabled using the CMake option mentioned above.
After make is run, the generated documentation files can then be found in the
html subdir of the build directory.

# Testing

The [Google Test framework](https://github.com/google/googletest) is used to
run various streaming tests. Make sure it's installed on the host system and then use the
following to build and run tests:

```shell
analog@analog:~$ cmake -DBUILD_TESTS=ON ..
analog@analog:~$ make check
```

Note that at least one device should be inserted to the system for the checks
to run properly.

# Python

Python Bindings are enabled by default and can be disabled using the CMake option mentioned above.

Note that this will build only one version of Python for the first supported
implementation it finds installed on the system. To build them for other
versions it's easiest to build them manually via the setup.py script in the
regular python manner if libsmu has already been built and/or installed on the
host machine.

```shell
analog@analog:~$ git clone https://github.com/analogdevicesinc/libsmu.git
analog@analog:~$ cd libsmu/bindings/python
analog@analog:~$ python3 setup.py build
analog@analog:~$ sudo python3 setup.py install
```

# Linux FAQ

By default, libsmu is installed into various directories inside /usr/local which
implies that the runtime linker cache often needs to be regenerated,
 otherwise runtime linking errors will occur.

Regenerate runtime linker cache after install:
```shell
analog@analog:~$ sudo ldconfig
```

If the same errors still happen, make sure the directory the libsmu library is
installed to is in the sourced files for /etc/ld.so.conf before running
ldconfig.

In addition, the udev rules file (53-adi-m1k-usb.rules) is installed by default
to give regular users access to devices supported by libsmu. Udev has to be
forced to reload its rules files in order to use the new file without rebooting
the system.

Reload udev rules files:
```shell
analog@analog:~$ sudo udevadm control --reload-rules
```

Finally, for python support on Debian/Ubuntu derived distros users will have to
export PYTHONPATH or perform a similar method since hand-built modules are
installed to the site-packages directory (which isn't in the standard search
list) while distro provided modules are placed in dist-packages.

Add pysmu module directory to python search path:
```
$ export PYTHONPATH=/usr/local/lib/python3.7/site-packages:${PYTHONPATH}
```

Note the command will have to be altered for targets with different bitness or
python versions.


# Windows

On Windows, it's easiest to use the provided installers,
[libsmu-setup-x86.exe](https://github.com/analogdevicesinc/libsmu/releases/latest) and
[libsmu-setup-x64.exe](https://github.com/analogdevicesinc/libsmu/releases/latest)
that install either 32 or 64 bit support, respectively. During the
install process options are provided to install drivers, Python bindings and Visual Studio
development support.
