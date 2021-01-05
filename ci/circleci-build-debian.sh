#!/usr/bin/env bash

#
# Build the Debian artifacts
#
set -xe
sudo apt-get -qq update
sudo apt-get install devscripts equivs

rm -rf build && mkdir build && cd build
mk-build-deps ../ci/control
sudo apt-get --allow-unauthenticated install ./*all.deb  || :
sudo apt-get --allow-unauthenticated install -f
rm -f ./*all.deb

tag=$(git tag --contains HEAD)

if [ -n "$BUILD_GTK3" ] && [ "$BUILD_GTK3" = "true" ]; then
  sudo update-alternatives --set wx-config /usr/lib/*-linux-*/wx/config/gtk3-unicode-3.0
fi

if [ -n "$tag" ]; then
  cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local ..
else
  cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=/usr/local ..
fi

make -j2
make package
ls -l
