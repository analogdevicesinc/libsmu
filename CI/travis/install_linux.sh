#!/bin/bash

if [[ ${OS_TYPE} == "doxygen" ]]; then make && sudo make install ; fi

if [[ ${OS_TYPE} == "doxygen" ]]; then
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
