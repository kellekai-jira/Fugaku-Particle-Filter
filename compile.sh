#!/bin/bash -e

#./uncrustify.sh
mkdir -p build
cd build
if [ "$(hostname)" == "narrenkappe" ];
then
    cmake .. -DZeroMQ_DIR=$HOME/workspace/melissa/install/share/cmake/ZeroMQ -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=install
else
    # works for intel:
    cmake .. -DZeroMQ_DIR=$HOME/zmq-install/share/cmake/ZeroMQ -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=install -DNETCDF_F_INCLUDE_DIR=/gpfs/software/juwels/stages/2019a/software/netCDF-Fortran/4.4.5-iimpi-2019a/include -DNETCDF_F_LIBRARY=/gpfs/software/juwels/stages/2019a/software/netCDF-Fortran/4.4.5-iimpi-2019a/lib/libnetcdff.a -DNETCDF_LIBRARY=/gpfs/software/juwels/stages/2019a/software/netCDF/4.6.3-iimpi-2019a/lib64/libnetcdf.a -DCMAKE_Fortran_COMPILER=ifort -DCMAKE_CXX_COMPILER=icpc -DCMAKE_C_COMPILER=icc
#cmake .. -DZeroMQ_DIR=$HOME/workspace/zmq/melissa-da/build/install/share/cmake/ZeroMQ -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=install -DCMAKE_CXX_COMPILER=$HOME/workspace/melissa-da/scalasca_cxx -DCMAKE_Fortran_COMPILER=$HOME/workspace/melissa-da/scalasca_f90
fi

make -j10 install

