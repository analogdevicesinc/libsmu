#!/bin/sh -e

PACKAGE_DIR=${1-build}
echo "==========================================================="$PACKAGE_DIR

apt-get -qq update
apt-get install -y git devscripts cython3 python3-setuptools python3-stdeb fakeroot dh-python python3-all debhelper python3-dev python3-all-dev python3-wheel libusb-1.0-0-dev libboost-all-dev

python -m pip install --upgrade pip
python -m pip install cmake setuptools wheel stdeb stdeb3 setuptools cython

cmake --version

cd $PACKAGE_DIR
PYTHON_INCLUDE_DIR=$(/usr/bin/python3 -c "from distutils.sysconfig import get_python_inc; print(get_python_inc())")  \
PYTHON_LIBRARY=$(/usr/bin/python3 -c "import distutils.sysconfig as sysconfig; print(sysconfig.get_config_var('LIBDIR'))")
PYTHON_EXECUTABLE=$(which /usr/bin/python3)
cmake -DENABLE_PACKAGING=ON -DBUILD_PYTHON=OFF -DPython_EXECUTABLE=$PYTHON_EXECUTABLE -DPython_INCLUDE_DIRS=$PYTHON_INCLUDE_DIR -DPython_LIBRARIES=$PYTHON_LIBRARY ..
make
make install

