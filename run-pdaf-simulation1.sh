#!/bin/bash


  # usage ./run.sh test <n_server> <n_simulation> <n_runners>

n_server=1
n_simulation=1
n_runners=1

ensemble_size=9
ensemble_size=3
total_steps=8  # TODO: I think totalsteps is not equal max_timestamp...

assimilator_type=0 # dummy
assimilator_type=1 # pdaf

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
rm -f nc.vg.*

rm -f *_ana.txt
rm -f *_for.txt

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
  echo compiling....
  set -e
  cd /home/friese/workspace/melissa-da/build
  make install
  cd -
  set +e
fi

lib_paths="/home/friese/workspace/melissa-da/build/install/lib:/home/friese/workspace/melissa/install/lib"
sim_exe="/home/friese/workspace/melissa-da/build/install/bin/pdaf-simulation1"
server_exe="/home/friese/workspace/melissa-da/build/install/bin/melissa_server"


killall xterm
killall lorenz_96
killall example_simulation


mpirun -n $n_server \
  -x LD_LIBRARY_PATH=$lib_paths \
  $precommand $server_exe $total_steps $ensemble_size $assimilator_type &

sleep 1


max_runner=`echo "$n_runners - 1" | bc`
for i in `seq 0 $max_runner`;
do
#  sleep 0.3  # use this and more than 100 time steps if you want to check for the start of propagation != 1... (having model task runners that join later...)
  #echo start simu id $i
  mpirun -n $n_simulation \
    -x MELISSA_SERVER_MASTER_NODE="tcp://narrenkappe:4000" \
    -x LD_LIBRARY_PATH=$lib_paths \
    $precommand $sim_exe &


  echo .
done


wait

