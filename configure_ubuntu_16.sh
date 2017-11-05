#!/usr/bin/env bash
echo "Starting GDB ImageWatch configuration for Ubuntu 16.04."
set -e
sudo apt-get install python3-pip
sudo apt-get -y install libpython3-dev python3-numpy python3-pip libfreetype6-dev libeigen3-dev
sudo pip3 install pysigset
mkdir build
cd build
export QT_SELECT=5
qmake ..
make -j$(nproc)
make install
cd ..
echo "source $(pwd)/build/gdb-imagewatch.py" > ~/.gdbinit
echo "Completed configuration of GDB ImageWatch."
