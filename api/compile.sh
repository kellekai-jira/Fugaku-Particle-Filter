#!/bin/bash

g++ -std=c++11 -c -fpermissive -fPIC  melissa_api.cxx -o test.o -lmpi -lzmq -I../../melissa/install/include -L../../melissa/install/lib -I/usr/include/mpi -L/usr/lib
