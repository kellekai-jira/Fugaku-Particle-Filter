#!/bin/bash -e
./uncrustify.sh
./compile.sh
./run-pdaf-simulation1.sh test
./test.sh
res1=$?

cd example-simulation
./test-fault-tolerance.sh
res2=$?

res=$((res1+res2))
echo passed 0=$res
if [ "$res" == "0" ];
then
  echo PASSED!
else
  echo FAILED!
fi
