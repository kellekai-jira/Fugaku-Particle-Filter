#!/bin/bash -e

#./uncrustify.sh
mkdir -p build
cd build
#cmake .. -DZeroMQ_DIR=$HOME/workspace/melissa/install/share/cmake/ZeroMQ -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=install
#cmake .. -DZeroMQ_DIR=$HOME/workspace/melissa/install/share/cmake/ZeroMQ -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=install
cmake ..  -DCMAKE_BUILD_TYPE=Releas -DCMAKE_INSTALL_PREFIX=install -DINSTALL_ZMQ=ON #-DZeroMQ_DIR=$HOME/workspace/melissa-da/zmq-install/share/cmake/ZeroMQ
make -j4 install

