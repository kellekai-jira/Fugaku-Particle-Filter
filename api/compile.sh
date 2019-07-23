#!/bin/bash
rm libmelissa_api.so
g++ -g -std=c++11 -fPIC  melissa_api.cxx -shared -o libmelissa_api.so -I../../melissa/install/include -I/usr/include/mpi
