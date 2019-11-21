#!/bin/bash

set -e
cd $MELISSA_DA_PATH/../../
./compile.sh
cd -
set +e

rm -f log
date >> log

start_time=`date +%s`

count=1

MAX_TIMESTAMP=300
MAX_TIMESTAMP=1400


MAX_ENSEMBLE_MEMBERS=5
MAX_SERVER_PROCS=4
MAX_SIMU_PROCS=4
MAX_SIMU_RUNNER=4

max_count=$(($MAX_ENSEMBLE_MEMBERS*$MAX_SERVER_PROCS*$MAX_SIMU_PROCS*$MAX_SIMU_RUNNER))

# figure out host:

if [[ `hostname | grep juwels.fzj.de | wc -l` != "0" ]];
then
  run_script="./run-juwels.sh"
else
  run_script="./run.sh"
fi

# run script:


#for ensemble_members in `seq 10 20 100`;
#do
  #for server_procs in `seq 1 $MAX_SERVER_PROCS`;
  #do
    #for simulation_procs in `seq 1 $MAX_SIMU_PROCS`;
    #do
      #for model_task_runners in `seq 1 $MAX_SIMU_RUNNER`;
      #do
# for juwels:
#for ensemble_members in 100 200 400 800 1600;
for ensemble_members in 100;
do
  for server_procs in 192;
  do
    for simulation_procs in 48;
    do
      #for model_task_runners in 1 2 4;   # works on 8 nodes on juwels...
      for model_task_runners in 2;   # works on 8 nodes on juwels...
      do
        echo "-----------------------------------------------------------------------------"
        echo step $count of $max_count:
        echo server ranks: $server_procs, simulation ranks: $simulation_procs, model runners: $model_task_runners
        echo ensemble members: $ensemble_members, max timestamp: $MAX_TIMESTAMP

        echo "-----------------------------------------------------------------------------" >> log
        echo step $count of $max_count: >> log
        echo server ranks: $server_procs, simulation ranks: $simulation_procs, model runners: $model_task_runners >> log
        echo ensemble members: $ensemble_members, max timestamp: $MAX_TIMESTAMP >> log
        time $run_script test $MAX_TIMESTAMP $ensemble_members $server_procs $simulation_procs $model_task_runners >> log

        count=$((count+1))
        sleep 1
      done
    done
  done
done

end_time=`date +%s`


runtime=$((end_time-start_time))

echo '-----------------------'
echo this took $runtime seconds!
