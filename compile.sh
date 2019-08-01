#!/bin/bash -e

CXX=mpicxx

cd /home/friese/workspace/melissa-da

# server
cd build_server
rm -f melissa_server
$CXX -std=c++0x -I/home/friese/workspace/melissa/install/include -O2 -g -Wall -c -fmessage-length=0 -fbounds-check -o server/server.o ../server/server.cxx
$CXX -L/home/friese/workspace/melissa/install/lib -o melissa_server server/server.o -lzmq
cd ..

# api
cd build_api
rm -f libmelissa_api.so
$CXX -std=c++0x -I/home/friese/workspace/melissa/install/include -O2 -g -Wall -c -fmessage-length=0 -fbounds-check -fPIC -o api/melissa_api.o ../api/melissa_api.cxx
$CXX -L/home/friese/workspace/melissa/install/lib -shared -o libmelissa_api.so api/melissa_api.o -lzmq
cd ..

cd build_example-simulation
# simulation
rm -f example_simulation1
$CXX -std=c++0x -I/home/friese/workspace/melissa-da -I/home/friese/workspace/melissa/install/include -O2 -g -Wall -c -fmessage-length=0 -fbounds-check -o example-simulation/simulation.o ../example-simulation/simulation.cxx
$CXX -L/home/friese/workspace/melissa-da/build_api -L/home/friese/workspace/melissa/install/lib -o example_simulation example-simulation/simulation.o -lmelissa_api -lzmq
cd ..

CXX=taucxx
# server
cd build_server

#a=$PWD
## mpicxx --showme:

#cp ../server/server.cxx .
#$CXX -std=c++0x -I/home/friese/workspace/melissa/install/include -O2 -g -Wall -c -fmessage-length=0 -fbounds-check -o server/server.o server.cxx `mpicxx --showme:compile`
#$CXX -L/home/friese/workspace/melissa/install/lib -o melissa_server server/server.o `mpicxx --showme:link` -lzmq
#cd ..
