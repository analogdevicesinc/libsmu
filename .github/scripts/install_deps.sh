#!/bin/sh -e

PACKAGE_DIR=${1-build}
echo $PACKAGE_DIR

apt-get -qq update
apt-get install -y git devscripts fakeroot libusb-1.0-0-dev libboost-all-dev python3 python3-dev python3-setuptools python3-pip python3-all

python3 --version
python3 -m pip install --upgrade pip
python3 -m pip install --upgrade cmake setuptools
cmake --version

cd $PACKAGE_DIR
python3 -m pip install setuptools build cython
PYTHON_INCLUDE_DIR=$(python3 -c "from distutils.sysconfig import get_python_inc; print(get_python_inc())")  \
PYTHON_LIBRARY=$(python3 -c "import distutils.sysconfig as sysconfig; print(sysconfig.get_config_var('LIBDIR'))")
PYTHON_EXECUTABLE=$(which python3)
echo "==== "$PYTHON_EXECUTABLE
echo "==== "$PYTHON_LIBRARY
echo "==== "$PYTHON_INCLUDE_DIR
cmake -DENABLE_PACKAGING=ON -DBUILD_PYTHON=ON -DPython_EXECUTABLE=$PYTHON_EXECUTABLE -DPython_INCLUDE_DIRS=$PYTHON_INCLUDE_DIR -DPython_LIBRARIES=$PYTHON_LIBRARY ..
cat setup.py
make
make install

