#!/bin/bash

OS_VERSION=$( /usr/libexec/PlistBuddy -c "Print:ProductVersion" /System/Library/CoreServices/SystemVersion.plist )
echo $OS_VERSION
mkdir -p build && cd build

# Build tar.gz for OSX (contains libraries and smu)
cmake -DBUILD_PYTHON=ON -DOSX_PACKAGE=ON -DCMAKE_OSX_DEPLOYMENT_TARGET=$OS_VERSION ..; make && sudo make install
