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
MAX_TIMESTAMP=100
MAX_TIMESTAMP=50
#MAX_TIMESTAMP=3


MAX_ENSEMBLE_MEMBERS=5
MAX_SERVER_PROCS=4
MAX_SIMU_PROCS=4
MAX_SIMU_RUNNER=5

max_count=$(($MAX_ENSEMBLE_MEMBERS*$MAX_SERVER_PROCS*$MAX_SIMU_PROCS*$MAX_SIMU_RUNNER))

# figure out host:

if [[ `hostname | grep juwels.fzj.de | wc -l` != "0" ]];
then
  run_script="./run-juwels.sh"
else
  run_script="./run.sh"
fi

#for convenient csv output:
echo 'cores simulation,number runners(max),cores server,runtime per iteration mean (ms),ensemble members,state size,iterations,mean bandwidth (MB/s),iterations used for means' >> output.csv



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
#for ensemble_members in 100;
#for ensemble_members in 1000;
for ensemble_members in 500;
#for ensemble_members in 1;
#for ensemble_members in 400;
do
  #for server_procs in 12;
  #for server_procs in 24 32 48 64 80 96;
  for server_procs in 80 96;
  do
    for simulation_procs in 48;
    do
      #for model_task_runners in 1 2 4;   # works on 8 nodes on juwels...
      #for model_task_runners in 7; # 2 4;   # works on 8 nodes on juwels...
      for model_task_runners in 6; # 2 4;   # works on 8 nodes on juwels...
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
        #time $run_script >> log

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
