#!/bin/bash

# build tar containg python files
if [[ ${OS_TYPE} != "doxygen" ]]; then 
	mkdir -p build_tar && cd build_tar
	cmake -DDEB_DETECT_DEPENDENCIES=ON -DENABLE_PACKAGING=ON -DBUILD_PYTHON=OFF ..; make package; rm *.deb; rm *.rpm; cd .. ; 
fi

mkdir -p build && cd build
cmake -DDEB_DETECT_DEPENDENCIES=ON -DENABLE_PACKAGING=ON -DBUILD_PYTHON=OFF ..; make package; rm *.tar.gz

if [[ ${OS_TYPE} == "doxygen" ]]; then "${BUILD_SOURCESDIRECTORY}/CI/travis/build_deploy_doc.sh" ; fi

if [[ ${OS_TYPE} != "doxygen" ]]; then make && sudo make install ; fi
