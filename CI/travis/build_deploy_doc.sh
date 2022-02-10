#!/bin/bash
cd ${BUILD_SOURCESDIRECTORY}
TOP_DIR=$(pwd)

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

	cd ${TOP_DIR}
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
pushd ${TOP_DIR}/build
(! make doc 2>&1 | grep -E "warning|error") || {
        echo_red "Documentation incomplete or errors in the generation of it have occured!"
        exit 1
}

echo_green "Documentation was generated successfully!"

############################################################################
# If the current build is not a pull request and it is on master the 
# documentation will be pushed to the gh-pages branch
############################################################################
if [[ "${IS_PULL_REQUEST}" == "False" && "${BRANCH_NAME}" == "master" ]]
then
        echo_green "Running Github docs update on commit '$CURRENT_COMMIT'"
        git config --global user.email "cse-ci-notifications@analog.com"
        git config --global user.name "CSE-CI"

        pushd ${TOP_DIR}
        git fetch --depth 1 origin +refs/heads/gh-pages:gh-pages
        git checkout gh-pages

        cp -R ${TOP_DIR}/build/doc/doxygen_doc/html/* ${TOP_DIR}

        sudo rm -rf ${TOP_DIR}/build/doc
        ls -l
        sudo rm -rf ${TOP_DIR}/build

        GH_CURRENT_COMMIT=$(git log -1 --pretty=%B)
        if [[ ${GH_CURRENT_COMMIT:(-7)} != ${CURRENT_COMMIT:0:7} ]]
        then
                git add --all .
                git commit --allow-empty --amend -m "Update documentation to ${CURRENT_COMMIT:0:7}"
                if [ -n "$GITHUB_DOC_TOKEN" ] ; then
                        git push https://${GITHUB_DOC_TOKEN}@github.com/${REPO_SLUG} gh-pages -f
                else
                        git push origin gh-pages -f
                fi
                echo_green "Documentation updated!"
        else
                echo_green "Documentation already up to date!"
        fi
else
        echo_green "Documentation will be updated when this commit gets on master!"
fi
