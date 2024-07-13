#!/usr/bin/env bash


# Build the  MacOS artifacts


# Copyright (c) 2021 Alec Leamas
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.

set -x

# Load local environment if it exists i. e., this is a local build
if [ -f ~/.config/local-build.rc ]; then source ~/.config/local-build.rc; fi


# If applicable,  restore /usr/local from cache.
if [[ -n "$CI" && -f /tmp/local.cache.tar ]]; then
  sudo rm -rf /usr/local/*
  sudo tar -C /usr -xf /tmp/local.cache.tar
fi

# Set up build directory
rm -rf build  && mkdir build

# Create a log file.
exec > >(tee build/build.log) 2>&1

export MACOSX_DEPLOYMENT_TARGET=10.10

# Return latest version of $1, optionally using option $2
pkg_version() { brew list --versions $2 $1 | tail -1 | awk '{print $2}'; }

#
# Check if the cache is with us. If not, re-install brew.
brew list --versions libexif || brew update-reset

# Install packaged dependencies
for pkg in cmake gettext libarchive libexif python3 wget openssl@3; do
    brew list --versions $pkg || brew install $pkg || brew install $pkg || :
    brew link --overwrite $pkg || brew install $pkg
done

# Custom additions
# Build the Kvaser and Rusoku Libraries
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


#Install python virtual environment
/usr/bin/python3 -m venv $HOME/cs-venv

#Install prebuilt dependencies
wget -q https://dl.cloudsmith.io/public/nohal/opencpn-plugins/raw/files/macos_deps_universal.tar.xz \
     -O /tmp/macos_deps_universal.tar.xz
sudo tar -C /usr/local -xJf /tmp/macos_deps_universal.tar.xz

export OPENSSL_ROOT_DIR='/usr/local'

# Build and package
cd build
cmake \
  "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE:-Release}" \
  -DCMAKE_INSTALL_PREFIX= \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=${MACOSX_DEPLOYMENT_TARGET} \
  -DOCPN_TARGET_TUPLE="darwin-wx32;10;universal" \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
  ..

if [[ -z "$CI" ]]; then
    echo '$CI not found in environment, assuming local setup'
    echo "Complete build using 'cd build; make tarball' or so."
    exit 0
fi

# nor-reproducible error on first invocation, seemingly tarball-conf-stamp
# is not created as required.
#make VERBOSE=1 tarball || make VERBOSE=1 tarball
make
make install
make package
make package

# Create the cached /usr/local archive
if [ -n "$CI"  ]; then
  tar -C /usr -cf /tmp/local.cache.tar  local
fi
