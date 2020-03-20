#!/bin/bash
# REM: only works if simulation timeout is set to 5 seconds on the serverside. otherwise
#      the timing here won't work!

echo REM: if this test fails just run it again. sometimes some logging is lost or some
echo killing does not work correctly due to race conditions.
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
# grepping iterations where at the start and at the end 8 runners were active... we start
# with 10 ;)
nb_killed=`grep -c '^199,\([0-9]*.[0-9]*,\)\{3\}8,8,.*$' log`
if [ "$nb_killed" != "1" ];
then
  echo ERROR: Something went wrong!
  echo ERROR: not exactly 2 model task runners were killed?
  echo DEBUG: maybe the regex turned wrong? Try cat log !
  exit 1
fi



./compare.sh reference-giraffe.txt
result=$?

# TODO: check timing info to see if really some runner were killed!
end_time=`date +%s`

runtime=$((end_time-start_time))
echo this took $runtime seconds!
exit $result
