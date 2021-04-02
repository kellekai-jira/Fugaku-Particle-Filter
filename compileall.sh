#!/bin/bash
srcdir=/gpfs/projects/bsc93/bsc93655/melissaP2P/Melissa/melissa-da
cd $srcdir/melissa
mkdir -p build
cd build
cmake .. -DINSTALL_ZMQ=1
#cmake -DZeroMQ_INCLUDE_DIR=$PROJECTS/melissaFT/opt/ZeroMQ/zeromq-marenostrum/install-4.3.4-marenostrum/include \
#    -DZeroMQ_LIBRARY=$PROJECTS/melissaFT/opt/ZeroMQ/zeromq-marenostrum/install-4.3.4-marenostrum/lib64/libzmq.so \
#    -DZeroMQ_VERSION="4.3.4" ..
make install
cd $srcdir
./compile.sh
echo -----------------------------------
