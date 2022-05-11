#!/usr/bin/env bash

#
# Build the  MacOS artifacts
#

set -xe

set -o pipefail
# Check if the cache is with us. If not, re-install brew.
brew list --versions libexif || brew update-reset

for pkg in cairo cmake gettext libarchive libexif python wget; do
    brew list --versions $pkg || brew install $pkg || brew install $pkg || :
    brew link --overwrite $pkg || brew install $pkg
done

# Force the MacOSX ennvironment for the two libraries
export MACOSX_DEPLOYMENT_TARGET=10.9
echo $MACOSX_DEPLOYMENT_TARGET
# Note to self, we use my fork of the libraries as the makefile has been changed with the
# addition of the cflag -mmacosx-deployment_target=10.9
# Build the Rusoku Toucan Library
git clone https://github.com/twocanplugin/rusokucan
cd rusokucan
# generates the build/version number for the Toucan library
./build_no.sh
# build the library
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
   # ln -s "data/drivers/${f##*/}" data/drivers/libTouCAN.dylib
   # there should only be one file, but in anycase exit after the first
  break
done

# Build the Kvaser Library
git clone https://github.com/twocanplugin/kvasercan-library
cd kvasercan-library
# generates the build/version number for the Kvaser library
./build_no.sh
# build the library
make all
# perhaps unnecessary
sudo make install
# copy include files - Note was cp -n Includes/*.h /usr/local/include 2>/dev/null || : to prevent over writing
sudo cp Includes/*.h /usr/local/include
# back to the project build directory
cd ..
# copy the resulting Kvaser library to the plugin data/drivers directory
# We will include the dylib file so that users do not have to compile/install 
# the dylibs themselves.
# It also allows the rpath to be correctly generated in the plugin dylib linker
for f in $(find kvasercan-library/Libraries/KvaserCAN  -name '*.dylib')
do
  echo $f
  cp $f data/drivers 
   # create a symbolic link to the library file
   # ln -s "data/drivers/${f##*/}" data/drivers/libKvaserCAN.dylib
   # there should only be one file, but in anycase exit after the first
  break
done


if [ -n "$WXVERSION" ] && [ "$WXVERSION" -eq "315" ]; then
    echo "Building for WXVERSION 315";
    WX_URL=https://download.opencpn.org/s/MCiRiq4fJcKD56r/download
    WX_DOWNLOAD=/tmp/wx315_opencpn50_macos1010.tar.xz
    WX_EXECUTABLE=/tmp/wx315_opencpn50_macos1010/bin/wx-config
    WX_CONFIG="--prefix=/tmp/wx315_opencpn50_macos1010"
    MACOSX_DEPLOYMENT_TARGET=10.10
else
    echo "Building for WXVERSION 312";
    WX_URL=https://download.opencpn.org/s/rwoCNGzx6G34tbC/download
    WX_DOWNLOAD=/tmp/wx312B_opencpn50_macos109.tar.xz
    WX_EXECUTABLE=/tmp/wx312B_opencpn50_macos109/bin/wx-config
    WX_CONFIG="--prefix=/tmp/wx312B_opencpn50_macos109"
    MACOSX_DEPLOYMENT_TARGET=10.9
fi

# Download required binaries using wget, since curl causes an issue with Xcode 13.1 and some specific certificates.
# Inspect the response code to see if the file is downloaded properly.
# If the download failed or file does not exist, then exit with an error.
# For local purposes: only download if it has not been downloaded already. That does not harm building on CircleCI.
if [ ! -f "$WX_DOWNLOAD" ]; then
  echo "Downloading $WX_DOWNLOAD";
  SERVER_RESPONSE=$(wget --server-response  -O $WX_DOWNLOAD $WX_URL 2>&1 | grep "HTTP"/ | awk '{print $2}')
  if [ $SERVER_RESPONSE -ne 200 ]; then
    echo "Fatal error: could not download $WX_DOWNLOAD. Server response: $SERVER_RESPONSE."
    exit 0
  fi
fi
if [ -f "$WX_DOWNLOAD" ]; then
  echo "$WX_DOWNLOAD exists"
else
  echo "Fatal error: $WX_DOWNLOAD does not exist";
  exit 0
fi

# Unpack the binaries to /tmp
tar xJf $WX_DOWNLOAD -C /tmp

# Extend PATH, only when necesary
INCLUDE_DIR_GETTEXT="/usr/local/opt/gettext/bin:"

if [[ ":$PATH:" != *$INCLUDE_DIR_GETTEXT* ]]; then
  echo "Your path is missing $INCLUDE_DIR_GETTEXT. Trying to add it automatically:"
  export PATH=$INCLUDE_DIR_GETTEXT$PATH
  echo 'export PATH="'$INCLUDE_DIR_GETTEXT'$PATH"' >> ~/.bash_profile
else
    echo "Path includes $INCLUDE_DIR_GETTEXT"
fi

export MACOSX_DEPLOYMENT_TARGET=$MACOSX_DEPLOYMENT_TARGET

# use brew to get Packages.pkg
if brew list --cask --versions packages; then
    version=$(brew list --cask --versions packages)
    version="${version/"packages "/}"
    sudo installer \
        -pkg /usr/local/Caskroom/packages/$version/packages/Packages.pkg \
        -target /
else
    brew install --cask packages
fi

rm -rf build && mkdir build && cd build
cmake \
  -DwxWidgets_CONFIG_EXECUTABLE=$WX_EXECUTABLE \
  -DwxWidgets_CONFIG_OPTIONS=$WX_CONFIG \
  -DCMAKE_INSTALL_PREFIX= \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=$MACOSX_DEPLOYMENT_TARGET \
  "/" \
  ..
make -sj2
make package

