#!/usr/bin/env bash

#
# Build the Debian artifacts
#
set -xe
sudo apt-get -qq update
sudo apt-get install devscripts equivs

rm -rf build && mkdir build && cd build

# Install extra libs
ME=$(echo ${0##*/} | sed 's/\.sh//g')
EXTRA_LIBS=../ci/extras/extra_libs.txt
if test -f "$EXTRA_LIBS"; then
    while read line; do
        sudo apt-get install $line
    done < $EXTRA_LIBS
fi
EXTRA_LIBS=../ci/extras/${ME}_extra_libs.txt
if test -f "$EXTRA_LIBS"; then
    while read line; do
        sudo apt-get install $line
    done < $EXTRA_LIBS
fi

mk-build-deps ../ci/control

sudo apt-get --allow-unauthenticated install ./*all.deb  || :
sudo apt-get --allow-unauthenticated install -f
rm -f ./*all.deb

tag=$(git tag --contains HEAD)

CMAKE_GTK3=""

if [ -n "$BUILD_GTK3" ] && [ "$BUILD_GTK3" = "true" ]; then
  sudo update-alternatives --set wx-config /usr/lib/*-linux-*/wx/config/gtk3-unicode-3.0
  CMAKE_GTK3="-DBUILD_GTK3=true"
fi

if [ -n "$tag" ]; then
  cmake -DCMAKE_BUILD_TYPE=Release $CMAKE_GTK3 -DCMAKE_INSTALL_PREFIX=/usr/local ..
else
  cmake -DCMAKE_BUILD_TYPE=Debug $CMAKE_GTK3 -DCMAKE_INSTALL_PREFIX=/usr/local ..
fi

make -j2
make package
ls -l
