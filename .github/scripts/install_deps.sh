#!/bin/sh -e

PACKAGE_DIR=${1-build}
echo "==========================================================="$PACKAGE_DIR

apt-get -qq update
apt-get install -y git devscripts fakeroot libusb-1.0-0-dev libboost-all-dev

python -m pip install --upgrade pip
python -m pip install --upgrade cmake setuptools setuptools

cmake --version

cd $PACKAGE_DIR
PYTHON_INCLUDE_DIR=$(/usr/bin/python3 -c "from distutils.sysconfig import get_python_inc; print(get_python_inc())")  \
PYTHON_LIBRARY=$(/usr/bin/python3 -c "import distutils.sysconfig as sysconfig; print(sysconfig.get_config_var('LIBDIR'))")
PYTHON_EXECUTABLE=$(which /usr/bin/python3)
cmake -DENABLE_PACKAGING=ON -DBUILD_PYTHON=OFF -DPython_EXECUTABLE=$PYTHON_EXECUTABLE -DPython_INCLUDE_DIRS=$PYTHON_INCLUDE_DIR -DPython_LIBRARIES=$PYTHON_LIBRARY ..
make
make install

