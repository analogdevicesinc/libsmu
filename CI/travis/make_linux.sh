#!/bin/bash

# build tar containg python files
if [[ ${OS_TYPE} != "doxygen" ]]; then 
	mkdir -p build_tar && cd build_tar
	cmake -DDEB_DETECT_DEPENDENCIES=ON -DENABLE_PACKAGING=ON -DUSE_PYTHON2=OFF -DBUILD_PYTHON=ON ..; make package; rm *.deb; rm *.rpm; cd .. ; 
fi

mkdir -p build && cd build
cmake -DDEB_DETECT_DEPENDENCIES=ON -DENABLE_PACKAGING=ON -DUSE_PYTHON2=OFF -DBUILD_PYTHON=OFF ..; make package; rm *.tar.gz

if [[ ${OS_TYPE} == "doxygen" ]]; then "${BUILD_SOURCESDIRECTORY}/CI/travis/build_deploy_doc.sh" ; fi

if [[ ${OS_TYPE} != "doxygen" ]]; then make && sudo make install ; fi

if [[ ${OS_TYPE} != "doxygen" ]]; then
	cd "${BUILD_SOURCESDIRECTORY}"/bindings/python
	python --version
	python -c "import struct; print(struct.calcsize('P') * 8)"
	python setup.py build_ext -L "${BUILD_SOURCESDIRECTORY}"/build/src
	python setup.py build
	python -c "from Cython.Build import cythonize"
	python setup.py bdist_wheel --skip-build
	sudo sed -i 's/my\ \$ignore_missing_info\ =\ 0;/my\ \$ignore_missing_info\ =\ 1;/' /usr/bin/dpkg-shlibdeps
	python setup.py --command-packages=stdeb.command sdist_dsc
	cd "$(find . -type d -name "debian" | head -n 1)"/..
	debuild -us -uc
	cp ${BUILD_SOURCESDIRECTORY}/bindings/python/deb_dist/*.deb ${BUILD_SOURCESDIRECTORY}/bindings/python
fi
