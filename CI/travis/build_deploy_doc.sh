#!/bin/bash


handle_doxygen() {
	# Install a recent version of doxygen
	DOXYGEN_URL="wget https://sourceforge.net/projects/doxygen/files/rel-1.8.15/doxygen-1.8.15.src.tar.gz"
	cd ${DEPS_DIR}
	[ -d "doxygen" ] || {
		mkdir doxygen && wget --quiet -O - ${DOXYGEN_URL} | tar --strip-components=1 -xz -C doxygen
	}
	cd doxygen
	mkdir -p build && cd build
	cmake ..
	make -j${NUM_JOBS}
	sudo make install
	cd ..
	cd ..

	cd ${TRAVIS_BUILD_DIR}
	mkdir -p build && cd build
	cmake -DWITH_DOC=ON ..
	cd ..
	cd ..
}

# Install Doxygen
handle_doxygen

echo_red() { printf "\033[1;31m$*\033[m\n"; }
echo_green() { printf "\033[1;32m$*\033[m\n"; }

############################################################################
# Check if the documentation will be generated w/o warnings or errors
############################################################################
pushd ${TRAVIS_BUILD_DIR}/build
(cd doc && ! make doc 2>&1 | grep -E "warning|error") || {
        echo_red "Documentation incomplete or errors in the generation of it have occured!"
        exit 1
}

echo_green "Documentation was generated successfully!"

############################################################################
# If the current build is not a pull request and it is on master the 
# documentation will be pushed to the gh-pages branch
############################################################################
if [[ "${TRAVIS_PULL_REQUEST}" == "false" && "${TRAVIS_BRANCH}" == "master" ]]
then
        pushd ${TRAVIS_BUILD_DIR}/build/doc
        git clone https://github.com/${TRAVIS_REPO_SLUG} --depth 1 --branch=gh-pages doc/html &>/dev/null

        pushd doc/html
        rm -rf *
        popd

        cp -R doxygen_doc/html/* doc/html/

        pushd doc/html
        CURRENT_COMMIT=$(git log -1 --pretty=%B)
        if [[ ${CURRENT_COMMIT:(-7)} != ${TRAVIS_COMMIT:0:7} ]]
        then
                git add --all .
                git commit --allow-empty --amend -m "Update documentation to ${TRAVIS_COMMIT:0:7}"
                git push https://${GITHUB_DOC_TOKEN}@github.com/${TRAVIS_REPO_SLUG} gh-pages -f &>/dev/null

                echo_green "Documetation updated!"
        else
                echo_green "Documentation already up to date!"
        fi
        popd
else
        echo_green "Documentation will be updated when this commit gets on master!"
fi