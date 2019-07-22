#!/bin/bash
rm test.o
g++ -std=c++11 -Wfatal-errors -c -fPIC  server.cxx -o test.o -lmpi -lzmq -I../../melissa/install/include -L../../melissa/install/lib -I/usr/include/mpi -L/usr/lib
