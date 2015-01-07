### LibSMU

LibSMU contains abstractions for streaming data to and from USB-connected analog interface devices, currently supporting the Nonolith CEE and Analog Devices' ADALM1000. Building off of LibUSB for cross-platform operation, it offers the sourcing of repeated waveforms, configuration of hardware, and measuring of signals.

This project also includes 'pysmu,' an initial binding of LibSMU for Python2.7.

#### Building LibSMU

With python2.7-dev, libusb-1.0, and clang++, building LibSMU and the python bindings is as simple as typing 'make.'

There's also a cmake description file, if you're in to that.

    mkdir build
    cd build
    cmake ..
    make
