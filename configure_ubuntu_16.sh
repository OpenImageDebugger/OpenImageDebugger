#!/usr/bin/env bash
echo "Starting GDB ImageWatch configuration for Ubuntu 16.04."
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
