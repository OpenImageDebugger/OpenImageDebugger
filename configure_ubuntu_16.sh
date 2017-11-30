#!/usr/bin/env bash
echo "Starting GDB ImageWatch configuration for Ubuntu 16.04."
set -e
sudo apt update
sudo apt install -y python3-pip
sudo apt install -y libpython3-dev libglew-dev python3-numpy python3-pip texinfo libfreetype6-dev libeigen3-dev
sudo -H pip3 install pysigset
mkdir -p build
cd build
export QT_SELECT=5
qmake ..
make -j$(nproc)
cd ..
echo "source $(pwd)/build/gdb-imagewatch.py" > ~/.gdbinit
echo "Completed configuration of GDB ImageWatch."
