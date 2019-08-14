#!/bin/bash -e


cd /home/friese/workspace/melissa-da
mkdir -p build
cd build
cmake .. -DZeroMQ_DIR=$HOME/workspace/melissa/install/share/cmake/ZeroMQ -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=install
make -j4 install

