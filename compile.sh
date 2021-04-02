#!/bin/bash -e

#./uncrustify.sh
mkdir -p build
cd build
#if [ "$ARCH" == "marenostrum" ];
#then
#    echo "INSTALL ON MARENOSTRUM..."
#    CC=gcc CXX=g++ FC=gfortran cmake .. -DWITH_FTI=ON -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=install \
#     -DWITH_FTI_THREADS=ON -DFTI_PATH=/gpfs/projects/bsc93/bsc93655/melissaP2P/FTI/fti/install \
#     -DREPORT_TIMING_ALL_RANKS=ON -DPDAF_PATH=$PROJECTS/melissaP2P/Melissa/PDAF \
#     -DProtobuf_INCLUDE_DIR=/apps/PROTOBUF/3.5.1/INTEL/include -DProtobuf_LIBRARY=/apps/PROTOBUF/3.5.1/INTEL/lib/libprotobuf.so \
#    # -DCMAKE_C_FLAGS=-D_POSIX_C_SOURCE=199309L \
#    # -DCMAKE_CXX_FLAGS="-D_GLIBCXX_USE_CXX11_ABI=0 -std=c++14"
#    #cmake .. -DWITH_FTI=ON -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=install -DHDF5_ROOT=/usr/lib/x86_64-linux-gnu/hdf5/openmpi \
#    # -DWITH_FTI_THREADS=ON -DFTI_PATH=/gpfs/projects/bsc93/bsc93655/melissaP2P/FTI/fti/install \
#    # -DZeroMQ_INCLUDE_DIR=$PROJECTS/melissaFT/opt/ZeroMQ/zeromq-marenostrum/install-4.3.4-marenostrum/include \
#    # -DZeroMQ_LIBRARY=$PROJECTS/melissaFT/opt/ZeroMQ/zeromq-marenostrum/install-4.3.4-marenostrum/lib64/libzmq.so \
#    # -DREPORT_TIMING_ALL_RANKS=ON -DPDAF_PATH=$PROJECTS/melissaP2P/Melissa/PDAF -DZeroMQ_VERSION="4.3.4" \
#    # -DProtobuf_INCLUDE_DIR=/apps/PROTOBUF/3.5.1/INTEL/include -DProtobuf_LIBRARY=/apps/PROTOBUF/3.5.1/INTEL/lib/libprotobuf.so \
#    # -DCMAKE_C_FLAGS=-D_POSIX_C_SOURCE=199309L \
#    # -DCMAKE_CXX_FLAGS="-D_GLIBCXX_USE_CXX11_ABI=0 -std=c++14" \
#
#
if [ "$(hostname)" == "narrenkappe" ];
then
    #cmake .. -DZeroMQ_DIR=$HOME/workspace/melissa/install/share/cmake/ZeroMQ -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=install
if [ "$MELISSA_PROFILING" == "" ];
then
    cmake .. -DINSTALL_FTI=ON -DWITH_FTI=ON -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=install -DHDF5_ROOT=/usr/lib/x86_64-linux-gnu/hdf5/openmpi \
     -DWITH_FTI_THREADS=ON \
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
    cmake .. -DINSTALL_FTI=ON -DWITH_FTI=OFF -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=install -DHDF5_ROOT=/usr/lib/x86_64-linux-gnu/hdf5/openmpi \
     -DWITH_FTI_THREADS=ON -DCMAKE_CXX_COMPILER="$CXX" -DCMAKE_C_COMPILER="$CC" -DCMAKE_Fortran_COMPILER="$F90"
fi
    #cmake .. -DWITH_FTI=OFF -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=install -DHDF5_ROOT=/usr/lib/x86_64-linux-gnu/hdf5/openmpi
elif [ "$USER" == "rkop006" ];
then
	# jean zay:
        cmake .. \
          -DCMAKE_BUILD_TYPE=Debug \
          -DCMAKE_INSTALL_PREFIX=install
else
    # juwels...
    if [ "$MELISSA_PROFILING" == "" ];
    then
        echo here, juwels...
        CC=mpicc CXX=mpic++ FC=mpifort cmake .. \
          -DCMAKE_BUILD_TYPE=Debug \
          -DCMAKE_INSTALL_PREFIX=install \
          -DCMAKE_CXX_COMPILER=mpicxx \
          -DCMAKE_Fortran_COMPILER=mpif90 \
          -DCMAKE_C_COMPILER=mpicc \
          -DINSTALL_FTI=OFF \
          -DWITH_FTI=ON \
          -DWITH_FTI_THREADS=ON \
          -DREPORT_TIMING_ALL_RANKS=ON \
          -DFTI_PATH=/gpfs/projects/bsc93/bsc93655/melissaP2P/FTI/fti/install \
          -DCMAKE_PREFIX_PATH=melissa/install/share/cmake/ZeroMQ \
          -DProtobuf_LIBRARIES=/apps/PROTOBUF/3.5.1/INTEL/lib/libprotobuf.so \
          -DProtobuf_INCLUDE_DIR=/apps/PROTOBUF/3.5.1/INTEL/include \
          -DCMAKE_CXX_FLAGS="-D_GLIBCXX_USE_CXX11_ABI=0 -std=c++14"

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

