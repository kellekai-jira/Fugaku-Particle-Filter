#!/bin/bash
rm server
mpicxx -g -std=c++11 -Wfatal-errors server.cxx -o server -lzmq -I../../melissa/install/include -L../../melissa/install/lib
