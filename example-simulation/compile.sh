#!/bin/bash
rm simulation
mpicxx -g -std=c++11 -Wfatal-errors simulation.cxx -o simulation -I ../api -I../../melissa/install/include -L../api -L../../melissa/install/lib -lmelissa_api -lzmq
