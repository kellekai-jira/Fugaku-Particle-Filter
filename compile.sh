#!/bin/bash -e

#./uncrustify.sh
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=install

make -j10 install

