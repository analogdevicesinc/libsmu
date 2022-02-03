#!/bin/bash

export DEPS_DIR="${BUILD_SOURCESDIRECTORY}/deps"
mkdir -p ${DEPS_DIR}

# Configure git user for Azure to push doc
git config --global user.name "Azure Bot"
git config --global user.email "<>"

brew update

brew install libusb
brew install python3
brew install boost
brew upgrade cmake

# for python bindings
python3 -m pip install --upgrade pip
python3 -m pip install twine build virtualenv wheel cython
