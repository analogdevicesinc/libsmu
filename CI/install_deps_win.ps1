
$ARCH=$Env:ARCH

git submodule update --init

echo "Downloading deps..."
cd C:\
wget http://swdownloads.analog.com/cse/build/libiio-win-deps.zip -OutFile "libiio-win-deps.zip"
7z x -y "C:\libiio-win-deps.zip"

echo "Downloading boost..."
wget https://boostorg.jfrog.io/artifactory/main/release/1.73.0/source/boost_1_73_0.7z -OutFile "boost.7z"
7z x -y "C:\boost.7z"

# Note: InnoSetup is already installed on Azure images; so don't run this step
#       Running choco seems a bit slow; seems to save about 40-60 seconds here
#choco install InnoSetup

set PATH=%PATH%;"C:\Program Files (x86)\Inno Setup 6"
