$COMPILER=$Env:COMPILER
$ARCH=$Env:ARCH

$src_dir=$pwd
python3 --version
cmake --version
python3 -m pip install --upgrade pip
python3 -m pip install wheel build virtualenv cython


cp .\dist\modpath.iss $env:BUILD_ARTIFACTSTAGINGDIRECTORY
if ($ARCH -eq "Win32") {
	echo "Running cmake for $COMPILER on 32 bit..."
	mkdir build
	cp .\dist\libsmu-x86.iss.cmakein .\build
	cd build

	cmake -G "$COMPILER" -A "$ARCH" -DCMAKE_SYSTEM_PREFIX_PATH="C:" -DLIBUSB_LIBRARIES="C:\\libs\\32\\libusb-1.0.lib" -DLIBUSB_INCLUDE_DIRS="C:\include\libusb-1.0" -DBOOST_ROOT="C:\\boost_1_73_0" -DBoost_USE_STATIC_LIBS=ON -DBUILD_STATIC_LIB=ON -DBUILD_EXAMPLES=ON -DBUILD_TESTS=ON -DBUILD_PYTHON=ON ..
	cmake --build . --config Debug
	cp .\dist\libsmu-x86.iss $env:BUILD_ARTIFACTSTAGINGDIRECTORY
} else {
    echo "Running cmake for $COMPILER on 64 bit..."
	mkdir build
	cp .\dist\libsmu-x64.iss.cmakein .\build
	cd build

	cmake -G "$COMPILER" -A "$ARCH" -DCMAKE_SYSTEM_PREFIX_PATH="C:" -DLIBUSB_LIBRARIES="C:\\libs\\64\\libusb-1.0.lib" -DLIBUSB_INCLUDE_DIRS="C:\include\libusb-1.0" -DBOOST_ROOT="C:\\boost_1_73_0" -DBoost_USE_STATIC_LIBS=ON -DBUILD_STATIC_LIB=ON -DBUILD_EXAMPLES=ON -DBUILD_TESTS=ON -DBUILD_PYTHON=ON ..
	cmake --build . --config Debug
	cp .\dist\libsmu-x64.iss $env:BUILD_ARTIFACTSTAGINGDIRECTORY
}
