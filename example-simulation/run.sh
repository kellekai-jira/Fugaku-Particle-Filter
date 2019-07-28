#!/bin/bash

  # usage ./run.sh test <n_server> <n_simulation> <n_runners>

n_server=1
n_simulation=2
n_runners=4

ensemble_size=5
max_timestamps=5

######################################################

# trap ctrl-c and call ctrl_c()
trap ctrl_c INT

function ctrl_c() {
        echo "** Trapped CTRL-C"
        killall xterm
        killall melissa_server
        killall example_simulation
        exit 0
}

precommand="xterm_gdb"
#precommand="xterm_gdb valgrind --leak-check=yes"
rm nc.vg.*

#precommand="xterm -e valgrind --track-origins=yes --leak-check=full --show-reachable=yes --log-file=nc.vg.%p"
#precommand="xterm -e valgrind --show-reachable=no --log-file=nc.vg.%p"
#precommand="xterm -e valgrind --vgdb=yes --vgdb-error=0 --leak-check=full --track-origins=yes --show-reachable=yes"
if [[ "$1" == "test" ]];
then
  # TODO: add ensemble size, max timesteps
  n_server=$2
  n_simulation=$3
  n_runners=$4

  echo testing with $n_server server procs and $n_runners times $n_simulation simulation nodes.

  precommand=""
else
  # compile:
  # abort on error!
  set -e
  ../compile.sh
  set +e

  echo running with $n_server server procs and $n_runners times $n_simulation simulation nodes.
fi

lib_paths="/home/friese/workspace/melissa-da/build_api:/home/friese/workspace/melissa/install/lib"
sim_exe="/home/friese/workspace/melissa-da/build_example-simulation/example_simulation"
server_exe="/home/friese/workspace/melissa-da/build_server/melissa_server"


killall xterm
killall melissa_server
killall example_simulation


rm output.txt

mpirun -n $n_server \
  -x LD_LIBRARY_PATH=$lib_paths \
  $precommand $server_exe &

sleep 1

max_runner=`echo "$n_runners - 1" | bc`
for i in `seq 0 $max_runner`;
do
  #sleep 0.5
  #echo start simu id $i
  mpirun -n $n_simulation \
    -x MELISSA_SIMU_ID=$i \
    -x MELISSA_SERVER_MASTER_NODE="tcp://narrenkappe:4000" \
    -x LD_LIBRARY_PATH=$lib_paths \
    $precommand $sim_exe &

#LD_PRELOAD=/usr/lib/valgrind/libmpiwrap-amd64-linux.so mpirun -n 4 -x MELISSA_SIMU_ID=$i -x MELISSA_SERVER_MASTER_NODE="tcp://narrenkappe:4000" -x LD_LIBRARY_PATH=/home/friese/workspace/melissa-da/build_api:/home/friese/workspace/melissa/install/lib $precommand /home/friese/workspace/melissa-da/build_example-simulation/example_simulation &

  echo .
done


wait

./compare.sh
exit $?

