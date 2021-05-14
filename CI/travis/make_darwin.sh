#!/bin/bash

mkdir -p build && cd build

# Build tar.gz for OSX (contains libraries and smu)
cmake -DENABLE_PACKAGING=ON -DBUILD_PYTHON=OFF ..; make package

# Build pkg for OSX (contains libsmu.framework - only base library)
cmake -DOSX_PACKAGE=ON -DENABLE_PACKAGING=OFF -DUSE_PYTHON2=OFF ..
make && sudo make install

cd "${BUILD_SOURCESDIRECTORY}"/bindings/python
python3 --version
python3 -c "import struct; print(struct.calcsize('P') * 8)"
python3 setup.py build_ext -L "${BUILD_SOURCESDIRECTORY}"/build/src
python3 setup.py build
python3 setup.py bdist_wheel --skip-build
