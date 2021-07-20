#!/usr/bin/env bash

#
# Build the  MacOS artifacts
#

# Fix broken ruby on the CircleCI image:
if [ -n "$CIRCLECI" ]; then
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install.sh)"
fi

set -xe

set -o pipefail
# Check if the cache is with us. If not, re-install brew.
brew list --versions libexif || brew update-reset

for pkg in cairo cmake gettext libarchive libexif python wget; do
    brew list --versions $pkg || brew install $pkg || brew install $pkg || :
    brew link --overwrite $pkg || brew install $pkg
done

# Build the Rusoku Toucan Library
git clone https://github.com/mac-can/rusokucan
cd rusokucan
# generates the build/version number for the Toucan library
./build_no.sh
make all
# perhaps unnecessary
sudo make install
# copy include files 
sudo cp Includes/*.h /usr/local/include
# back to the project build directory
cd ..
# copy the resulting Toucan library to the plugin data/drivers directory
# We will include the dylib file so that users do not have to compile/install 
# the dylibs themselves.
# It also allows the rpath to be correctly generated in the plugin dylib linker
for f in $(find rusokucan/Libraries/TouCAN  -name '*.dylib')
do
  echo $f
  cp $f data/drivers 
   # create a symbolic link to the library file
   ln -s "data/drivers/${f##*/}" data/drivers/libTouCAN.dylib
   # there should only be one file, but in anycase exit after the first
  break
done

# Build the Kvaser Library
git clone https://github.com/mac-can/MacCAN-KvaserCAN
cd MacCAN-KvaserCAN
# generates the build/version number for the Toucan library
./build_no.sh
make all
# perhaps unnecessary
sudo make install
# copy include files - note not overwriting the headers from the rusoku driver
# suppress error code and exit
sudo cp -n Includes/*.h /usr/local/include 2>/dev/null || :
# back to the project build directory
cd ..
# copy the resulting Toucan library to the plugin data/drivers directory
# We will include the dylib file so that users do not have to compile/install 
# the dylibs themselves.
# It also allows the rpath to be correctly generated in the plugin dylib linker
for f in $(find MacCAN-KvaserCAN/Libraries/KvaserCAN  -name '*.dylib')
do
  echo $f
  cp $f data/drivers 
   # create a symbolic link to the library file
   ln -s "data/drivers/${f##*/}" data/drivers/libKvaserCAN.dylib
   # there should only be one file, but in anycase exit after the first
  break
done

#wget -q http://opencpn.navnux.org/build_deps/wx312_opencpn50_macos109.tar.xz - unsupported link, replaced with below
curl -o wx312B_opencpn50_macos109.tar.xz https://download.opencpn.org/s/rwoCNGzx6G34tbC/download
tar xJf wx312B_opencpn50_macos109.tar.xz -C /tmp
export PATH="/usr/local/opt/gettext/bin:$PATH"
echo 'export PATH="/usr/local/opt/gettext/bin:$PATH"' >> ~/.bash_profile

export MACOSX_DEPLOYMENT_TARGET=10.9

#wget -q http://opencpn.navnux.org/build_deps/Packages.dmg - unsupported link, replaced with below
#curl -o Packages.dmg https://download.opencpn.org/s/SneCR3z9XM3aRc6/download
#hdiutil attach Packages.dmg
#sudo installer -pkg "/Volumes/Packages 1.2.5/Install Packages.pkg" -target "/"
# use brew to get Packages.pkg
if brew list --cask --versions packages; then
    version=$(pkg_version packages '--cask')
    sudo installer \
        -pkg /usr/local/Caskroom/packages/$version/packages/Packages.pkg \
        -target /
else
    brew install --cask packages
fi

rm -rf build && mkdir build && cd build
cmake \
  -DwxWidgets_CONFIG_EXECUTABLE=/tmp/wx312B_opencpn50_macos109/bin/wx-config \
  -DwxWidgets_CONFIG_OPTIONS="--prefix=/tmp/wx312B_opencpn50_macos109" \
  -DCMAKE_INSTALL_PREFIX= \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=10.9 \
  "/" \
  ..
make -sj2
make package

make create-pkg
