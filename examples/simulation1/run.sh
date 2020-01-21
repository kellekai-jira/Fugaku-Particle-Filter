#!/bin/bash

  # usage ./run.sh test <n_server> <n_simulation> <n_runners>

n_server=3
n_simulation=3
n_runners=4

n_server=1
n_simulation=2
n_runners=1

ensemble_size=5
total_steps=1440

######################################################

# trap ctrl-c and call ctrl_c()
trap ctrl_c INT

function ctrl_c() {
        echo "** Trapped CTRL-C"
        killall xterm
        killall $server_exe
        killall $sim_exe
        exit 0
}

#precommand="xterm_gdb"
#precommand="xterm_gdb valgrind --leak-check=yes"
rm -f nc.vg.*

#precommand="xterm -e valgrind --track-origins=yes --leak-check=full --show-reachable=yes --log-file=nc.vg.%p"
#precommand="xterm -e valgrind --show-reachable=no --log-file=nc.vg.%p"
#precommand="xterm -e valgrind --vgdb=yes --vgdb-error=0 --leak-check=full --track-origins=yes --show-reachable=yes"
if [[ "$1" == "test" ]];
then
  # TODO: add ensemble size, max timesteps
  total_steps=$2
  ensemble_size=$3
  n_server=$4
  n_simulation=$5
  n_runners=$6


  echo testing with $n_server server procs and $n_runners times $n_simulation simulation nodes.

  precommand=""
else
  # compile:
  # abort on error!
  set -e
  cd ../..
  ./compile.sh
  cd -
  set +e

  echo running with $n_server server procs and $n_runners times $n_simulation simulation nodes.
fi

source ../../build/install/bin/melissa-da_set_env.sh

bin_path="$MELISSA_DA_PATH/bin"

server_exe="melissa_server"
sim_exe="simulation1"

killall xterm
killall $server_exe
killall $sim_exe

server_exe_path="$bin_path/$server_exe"
sim_exe_path="$bin_path/$sim_exe"


rm output.txt

$MPIEXEC -n $n_server \
  -x LD_LIBRARY_PATH=$LD_LIBRARY_PATH \
  $precommand $server_exe_path $total_steps $ensemble_size &
sleep 1

max_runner=`echo "$n_runners - 1" | bc`
for i in `seq 0 $max_runner`;
do
#  sleep 0.3  # use this and more than 100 time steps if you want to check for the start of propagation != 1... (having model task runners that join later...)
  #echo start simu id $i
  $MPIEXEC -n $n_simulation \
    -x MELISSA_SERVER_MASTER_NODE="tcp://localhost:4000" \
    -x LD_LIBRARY_PATH=$LD_LIBRARY_PATH \
    $precommand $sim_exe_path &>/dev/null &

#LD_PRELOAD=/usr/lib/valgrind/libmpiwrap-amd64-linux.so $MPIEXEC -n 4 -x MELISSA_SIMU_ID=$i -x MELISSA_SERVER_MASTER_NODE="tcp://narrenkappe:4000" -x LD_LIBRARY_PATH=/home/friese/workspace/melissa-da/build_api:/home/friese/workspace/melissa/install/lib $precommand /home/friese/workspace/melissa-da/build_example-simulation/simulation1 &

  echo .
done


wait

