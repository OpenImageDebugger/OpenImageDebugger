#!/usr/bin/env bash

set -e

echo -e "\tThis version of Open Image Debugger requires Qt 5.6 (or more recent)."
echo -e "\tIf you don't have it, please download it from https://www.qt.io/download"
echo "Starting Open Image Debugger configuration for Ubuntu 16.04."
git submodule init
git submodule update
set -e
sudo apt-get -y install build-essential libpython3-dev python3-dev libpython2.7-dev python2-dev
mkdir build
cd build
export QT_SELECT=5
qmake ..
make -j$(nproc)
make install
cd ..
echo "source $(pwd)/build/oid.py" > ~/.gdbinit
echo "Completed configuration of Open Image Debugger."
