#!/bin/bash

set -e
../compile.sh
set +e

rm -f log
date >> log

start_time=`date +%s`

count=1

MAX_SERVER_PROCS=3
MAX_SIMU_PROCS=3
MAX_SIMU_RUNNER=3
max_count=$(($MAX_SERVER_PROCS*$MAX_SIMU_PROCS*$MAX_SIMU_RUNNER))

function check {
  res=$1
  echo .
  echo .
  if [[ "$res" == "0" ]];
  then
    echo PASSED!
  else
    echo ERROR!
    exit 1
  fi
}

MAX_TIMESTAMP=5
ENSEMBLE_MEMBERS=5

for server_procs in `seq 1 $MAX_SERVER_PROCS`;
do
  for simulation_procs in `seq 1 $MAX_SIMU_PROCS`;
  do
    for model_task_runners in `seq 1 $MAX_SIMU_RUNNER`;
    do
      echo "-----------------------------------------------------------------------------"
      echo step $count of $max_count:
      echo server ranks: $server_procs, simulation ranks: $simulation_procs, model runners: $model_task_runners
      echo ensemble members: $ENSEMBLE_MEMBERS, max timestamp: $MAX_TIMESTAMP

      echo "-----------------------------------------------------------------------------" >> log
      echo step $count of $max_count: >> log
      echo server ranks: $server_procs, simulation ranks: $simulation_procs, model runners: $model_task_runners >> log
      echo ensemble members: $ENSEMBLE_MEMBERS, max timestamp: $MAX_TIMESTAMP >> log
      time ./run.sh test $MAX_TIMESTAMP $ENSEMBLE_MEMBERS $server_procs $simulation_procs $model_task_runners >> log

      ./compare.sh reference.txt &>/dev/null
      check $?
      count=$((count+1))
    done
  done
done

end_time=`date +%s`


runtime=$((end_time-start_time))

echo '-----------------------'
echo check that at least one simulation registered later:
grep 'First timestep to propagate:' log | cut -d' ' -f5 | grep -v 1
check $?

echo .
echo .
echo ===================================
echo PASSED !
echo this took $runtime seconds!
