$src_dir=$pwd
$ARCH=$Env:ARCH

cd 'C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Redist\MSVC\14.29.30133'
if ($ARCH -eq "Win32") {
	echo "$PWD"
	cp .\x86\Microsoft.VC142.CRT\msvcp140.dll $env:BUILD_ARTIFACTSTAGINGDIRECTORY
	cp .\x86\Microsoft.VC142.CRT\vcruntime140.dll $env:BUILD_ARTIFACTSTAGINGDIRECTORY
	cp .\x86\Microsoft.VC142.OpenMP\vcomp140.dll $env:BUILD_ARTIFACTSTAGINGDIRECTORY
	cp -R 'C:\Program Files (x86)\Windows Kits\10\Redist\ucrt\DLLs\x86\*' $env:BUILD_ARTIFACTSTAGINGDIRECTORY
}else {
	echo "$PWD"
    cp .\x64\Microsoft.VC142.CRT\msvcp140.dll $env:BUILD_ARTIFACTSTAGINGDIRECTORY
	cp .\x64\Microsoft.VC142.CRT\vcruntime140.dll $env:BUILD_ARTIFACTSTAGINGDIRECT
	cp .\x64\Microsoft.VC142.CRT\vcruntime140_1.dll $env:BUILD_ARTIFACTSTAGINGDIRECT
	cp .\x64\Microsoft.VC142.OpenMP\vcomp140.dll $env:BUILD_ARTIFACTSTAGINGDIRECTORY
	cp -R 'C:\Program Files (x86)\Windows Kits\10\Redist\ucrt\DLLs\x64\*' $env:BUILD_ARTIFACTSTAGINGDIRECTORY
}

cd $src_dir
mkdir dependencies
cd dependencies
wget http://swdownloads.analog.com/cse/build/libiio-win-deps.zip -OutFile "libiio-win-deps.zip"
7z x -y "libiio-win-deps.zip"

echo "Downloading dpinst..."
wget http://swdownloads.analog.com/cse/m1k/drivers/dpinst.zip -OutFile "dpinst.zip"
7z x -y "dpinst.zip"

cp -R ..\src\ $env:BUILD_ARTIFACTSTAGINGDIRECTORY
cp -R ..\include\ $env:BUILD_ARTIFACTSTAGINGDIRECTORY
cp .\include\libusb-1.0\libusb.h $env:BUILD_ARTIFACTSTAGINGDIRECTORY\include\libsmu\

if (!(Test-Path $env:BUILD_ARTIFACTSTAGINGDIRECTORY\drivers)) {
	mkdir $env:BUILD_ARTIFACTSTAGINGDIRECTORY\drivers
}
cp ..\dist\m1k-winusb.* $env:BUILD_ARTIFACTSTAGINGDIRECTORY\drivers\
cp -R ..\dist\x86 $env:BUILD_ARTIFACTSTAGINGDIRECTORY\drivers
cp -R ..\dist\amd64 $env:BUILD_ARTIFACTSTAGINGDIRECTORY\drivers

if ($ARCH -eq "Win32") {
	cp .\libs\32\libusb-1.0.dll $env:BUILD_ARTIFACTSTAGINGDIRECTORY
	cp .\dpinst.exe $env:BUILD_ARTIFACTSTAGINGDIRECTORY\drivers
} else {
    cp .\libs\64\libusb-1.0.dll $env:BUILD_ARTIFACTSTAGINGDIRECTORY
	cp .\dpinst_amd64.exe $env:BUILD_ARTIFACTSTAGINGDIRECTORY\drivers
}
