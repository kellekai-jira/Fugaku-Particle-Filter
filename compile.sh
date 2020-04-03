#!/bin/bash -e

#./uncrustify.sh
mkdir -p build
cd build
if [ "$(hostname)" == "narrenkappe" ];
then
    #cmake .. -DZeroMQ_DIR=$HOME/workspace/melissa/install/share/cmake/ZeroMQ -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=install
    cmake .. -DINSTALL_FTI=ON -DWITH_FTI=ON -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=install -DHDF5_ROOT=/usr/lib/x86_64-linux-gnu/hdf5/openmpi \
    -DWITH_FTI=ON -DWITH_FTI_THREADS=ON
else
    cmake .. -DZeroMQ_DIR=$HOME/workspace/zmq/melissa-da/build/install/share/cmake/ZeroMQ -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=install
#cmake .. -DZeroMQ_DIR=$HOME/workspace/zmq/melissa-da/build/install/share/cmake/ZeroMQ -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=install -DCMAKE_CXX_COMPILER=$HOME/workspace/melissa-da/scalasca_cxx -DCMAKE_Fortran_COMPILER=$HOME/workspace/melissa-da/scalasca_f90
fi

make -j10 install

