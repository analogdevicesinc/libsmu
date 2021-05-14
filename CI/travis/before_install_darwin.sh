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
sudo easy_install pip
pip3 install --only-binary ":all:" --disable-pip-version-check --upgrade pip
sudo pip3 install --only-binary ":all:" wheel cython
