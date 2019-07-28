#!/bin/bash

rm -f log
date >> log

for server_procs in `seq 1 5`;
do
  for simulation_procs in 1 2 4;
  do
    for model_task_runners in `seq 1 3`;
    do
      echo server ranks: $server_procs, simulation ranks: $simulation_procs, model runners: $model_task_runners
      echo "-----------------------------------------------------------------------------" >> log
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
    done
  done
done
