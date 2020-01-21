#!/bin/bash -e
./uncrustify.sh
./compile.sh
./run-simulation2-pdaf.sh test
./test.sh
res1=$?

cd examples/simulation1
./test-fault-tolerance.sh
res2=$?


# check if simulation stateless check works

cd examples
./check_stateless.sh ../build/install/bin/simulation1
res3=$?

./check_stateless.sh ../build/install/bin/simulation1-stateful
if $?
then
  res4=1
else
  # suppose it to fail as stateful
  res4=0
fi



res=$((res1+res2+res3+res4))
echo passed 0=$res
if [ "$res" == "0" ];
then
  echo PASSED!
else
  echo FAILED!
fi
