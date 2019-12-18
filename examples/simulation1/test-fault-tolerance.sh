#!/bin/bash
# REM: only works if simulation timeout is set to 5 seconds on the serverside. otherwise
#      the timing here won't work!

cd ../..
./compile.sh
cd -

start_time=`date +%s`

./run.sh test 200 42 3 2 10 >log &
sleep 2
./killing-giraffe.sh simulation1
sleep 7
./killing-giraffe.sh simulation1

wait

# ensure 2 runners were really killed:
nb_killed=`grep "^199,[0-9.]*,8,8$" log | wc -l`
if [ "$nb_killed" != "1" ];
then
  echo ERROR: Something went wrong!
  echo ERROR: not exactly 2 model task runners were killed?
  exit 1
fi



./compare.sh reference-giraffe.txt
result=$?

# TODO: check timing info to see if really some runner were killed!
end_time=`date +%s`

runtime=$((end_time-start_time))
echo this took $runtime seconds!
exit $result
