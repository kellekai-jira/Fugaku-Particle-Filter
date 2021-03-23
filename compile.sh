#!/bin/bash -e

#./uncrustify.sh
mkdir -p build
cd build
if [ "$(hostname)" == "narrenkappe" ];
then
    #cmake .. -DZeroMQ_DIR=$HOME/workspace/melissa/install/share/cmake/ZeroMQ -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=install
if [ "$MELISSA_PROFILING" == "" ];
then
    cmake .. -DINSTALL_FTI=OFF -DWITH_FTI=ON -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=install -DHDF5_ROOT=/usr/lib/x86_64-linux-gnu/hdf5/openmpi \
     -DWITH_FTI_THREADS=ON \
     -DREPORT_TIMING_ALL_RANKS=ON \
    -DFTI_PATH=$HOME/tmp/FTI
    #cmake .. -DWITH_FTI=OFF -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=install \
     #-DREPORT_TIMING_ALL_RANKS=ON
else
    F90="wrapper-f90.sh"
    CC="wrapper-cc.sh"
    CXX="wrapper-cxx.sh"

    #F90=scorep-gfortran
    #CC=scorep-gcc
    #CXX=scorep-g++
    #make SCOREP_WRAPPER_INSTRUMENTER_FLAGS="--user" SCOREP_WRAPPER_COMPILER_FLAGS="-g –O2"
    cd ..
    export PATH="$PWD/profiling:$PATH"
    cd build
    cmake .. -DINSTALL_FTI=OFF -DWITH_FTI=OFF -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=install -DHDF5_ROOT=/usr/lib/x86_64-linux-gnu/hdf5/openmpi \
     -DWITH_FTI_THREADS=ON -DCMAKE_CXX_COMPILER="$CXX" -DCMAKE_C_COMPILER="$CC" -DCMAKE_Fortran_COMPILER="$F90"
fi
    #cmake .. -DWITH_FTI=OFF -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=install -DHDF5_ROOT=/usr/lib/x86_64-linux-gnu/hdf5/openmpi
elif [ "$USER" == "rkop006" ];
then
	# jean zay:
        cmake .. \
          -DCMAKE_BUILD_TYPE=Debug \
          -DCMAKE_INSTALL_PREFIX=install
elif [ "$(hostname)" == "linux-rol5" ];
then
    #cmake .. -DZeroMQ_DIR=$HOME/workspace/melissa/install/share/cmake/ZeroMQ -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=install
if [ "$MELISSA_PROFILING" == "" ];
then
    cmake .. -DINSTALL_FTI=OFF -DWITH_FTI=ON -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=install -DHDF5_ROOT=/home/$USER/opt/HDF5/GCC/1.10.5 \
     -DWITH_FTI_THREADS=ON \
     -DFTI_PATH=/home/kellekai/STUDY/PhD/Research/Melissa-P2P-Lab/FTI/fti-gitlab/install \
     -DREPORT_TIMING_ALL_RANKS=ON
    #cmake .. -DWITH_FTI=OFF -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=install \
     #-DREPORT_TIMING_ALL_RANKS=ON
else
    F90="wrapper-f90.sh"
    CC="wrapper-cc.sh"
    CXX="wrapper-cxx.sh"

    #F90=scorep-gfortran
    #CC=scorep-gcc
    #CXX=scorep-g++
    #make SCOREP_WRAPPER_INSTRUMENTER_FLAGS="--user" SCOREP_WRAPPER_COMPILER_FLAGS="-g –O2"
    cd ..
    export PATH="$PWD/profiling:$PATH"
    cd build
    cmake .. -DINSTALL_FTI=ON -DWITH_FTI=OFF -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=install -DHDF5_ROOT=/home/kellekai/opt/HDF5/GCC/1.10.5 \
     -DWITH_FTI_THREADS=ON -DCMAKE_CXX_COMPILER="$CXX" -DCMAKE_C_COMPILER="$CC" -DCMAKE_Fortran_COMPILER="$F90"
fi
else
    # juwels...
    if [ "$MELISSA_PROFILING" == "" ];
    then
        echo here, juwels...
        cmake .. \
          -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_INSTALL_PREFIX=install \
          -DCMAKE_CXX_COMPILER=mpicxx \
          -DCMAKE_Fortran_COMPILER=mpif90 \
          -DCMAKE_C_COMPILER=mpicc \
          -DZeroMQ_ROOT=$HOME/workspace/melissa-da/melissa/install \
          -DSLOW_MPI_DIR="/p/scratch/prcoe03/tmp"
    else
        echo here, juwels with profiling...
        cd ..
        export PATH="$PWD/profiling:$PATH"
        cd build
        F90="wrapper-ifort.sh"
        CC="wrapper-icc.sh"
        CXX="wrapper-icpc.sh"
        cmake .. \
            -DCMAKE_BUILD_TYPE=RelWithDebInfo \
            -DCMAKE_INSTALL_PREFIX=install \
            -DCMAKE_CXX_COMPILER="$CXX" \
            -DCMAKE_C_COMPILER="$CC" \
            -DCMAKE_Fortran_COMPILER="$F90" \
            -DZeroMQ_ROOT=$HOME/workspace/melissa-da/melissa/install
            #-DCMAKE_CXX_COMPILER_WORKS=1 \
            #-DCMAKE_C_COMPILER_WORKS=1 \
            #-DCMAKE_Fortran_COMPILER_WORKS=1
        # cmake on speed^ don't do slow compiler checking over and over again :P

        make install -j 20 && cd ../profiling && ./trick-melissa-api.sh && exit 0

    fi
#cmake .. -DZeroMQ_DIR=$HOME/workspace/zmq/melissa-da/build/install/share/cmake/ZeroMQ -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=install -DCMAKE_CXX_COMPILER=$HOME/workspace/melissa-da/scalasca_cxx -DCMAKE_Fortran_COMPILER=$HOME/workspace/melissa-da/scalasca_f90
fi

make install -j48
echo -----------------------------------

