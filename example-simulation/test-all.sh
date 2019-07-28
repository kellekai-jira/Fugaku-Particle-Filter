#!/bin/bash

set -e
../compile.sh
set +e

rm -f log
date >> log

start_time=`date +%s`

count=1

MAX_SERVER_PROCS=5
MAX_SIMU_PROCS=4
MAX_SIMU_RUNNER=5
max_count=$(($MAX_SERVER_PROCS*$MAX_SIMU_PROCS*$MAX_SIMU_RUNNER))

for server_procs in `seq 1 $MAX_SERVER_PROCS`;
do
  for simulation_procs in `seq 1 $MAX_SIMU_PROCS`;
  do
    for model_task_runners in `seq 1 $MAX_SIMU_RUNNER`;
    do
      echo "-----------------------------------------------------------------------------"
      echo step $count of $max_count:
      echo server ranks: $server_procs, simulation ranks: $simulation_procs, model runners: $model_task_runners

      echo "-----------------------------------------------------------------------------" >> log
      echo step $count of $max_count: >> log
      echo server ranks: $server_procs, simulation ranks: $simulation_procs, model runners: $model_task_runners >> log
      time ./run.sh test $server_procs $simulation_procs $model_task_runners >> log

      res=$?
      echo .
      echo .
      if [[ "$res" == "0" ]];
      then
        echo PASSED!
      else
        echo ERROR!
        exit 1
      fi
      count=$((count+1))
    done
  done
done

end_time=`date +%s`


runtime=$((end_time-start_time))

echo .
echo .
echo ===================================
echo PASSED !
echo this took $runtime seconds!
