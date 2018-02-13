#!/usr/bin/env bash
echo -e "\tThis version of GDB ImageWatch requires Qt 5.6 (or more recent)."
echo -e "\tIf you don't have it, please download it from https://www.qt.io/download"
echo "Starting GDB ImageWatch configuration for Ubuntu 16.04."
git submodule init
git submodule update
set -e
sudo apt-get -y install libpython3-dev
mkdir build
cd build
export QT_SELECT=5
qmake ..
make -j$(nproc)
make install
cd ..
echo "source $(pwd)/build/gdb-imagewatch.py" > ~/.gdbinit
echo "Completed configuration of GDB ImageWatch."
