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
If you want to install pysmu (the Python bindings for libsmu), you can download the specific wheel for your version.
We provide python wheel packages for the following Python versions: py3.7, py3.8, py3.9, py3.10. You can download the .whl
for your Python version from the official releases or use the ones provided on test.pypi.org (soon from the official pypi.org as well).
On Linux:
```shell
# If you are installing from test.pypi.org:
analog@analog:~$ python3 -m pip install --index-url https://test.pypi.org/simple/ pysmu
# If you are installing the .whl downloaded from our official github release:
analog@analog:~$ python3 -m pip install pysmu-1.x.y-cp3x-cp3x-your-os-type.whl
```
Please note that in order to use these bindings you need the core C++ library they depend upon. This is not packaged with the pypi release but you can install the [latest release](https://github.com/analogdevicesinc/libsmu/releases/latest) or the latest **untested** binaries from the [master branch](https://ci.appveyor.com/project/analogdevicesinc/libsmu).

If you want to build them manually, please check the [build guide](https://wiki.analog.com/university/tools/m1k/libsmu#how_to_build_it) for your specific operating system.
