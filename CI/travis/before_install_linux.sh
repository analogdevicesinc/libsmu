#!/bin/bash

export DEPS_DIR="${BUILD_SOURCESDIRECTORY}/deps"
mkdir -p ${DEPS_DIR}

# Configure git user for Azure to push doc
git config --global user.name "Azure Bot"
git config --global user.email "<>"

sudo apt-get update

# for python bindings
if [[ ${OS_TYPE} != "doxygen" ]]; then pip install --only-binary ":all:" --disable-pip-version-check --upgrade pip ; fi
if [[ ${OS_TYPE} != "doxygen" ]]; then python -m pip install --upgrade pip stdeb stdeb3 setuptools cython ; fi
if [[ ${OS_TYPE} != "doxygen" ]]; then python -m pip install setuptools wheel stdeb ; fi

sudo apt-get install devscripts cython3 python3-setuptools python3-stdeb fakeroot dh-python python3-all debhelper python3-dev python3-all-dev python3-wheel libusb-1.0-0-dev libboost-all-dev
