#!/bin/bash -e
./uncrustify.sh
./compile.sh
./run-simulation2-pdaf.sh test
./test.sh
res1=$?

cd examples/simulation1
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
