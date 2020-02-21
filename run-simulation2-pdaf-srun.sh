#!/bin/bash


if [ -z "$SLURM_JOB_ID" ];
then
    echo 'I need to be started in a slurm allocation (use e.g. salloc)'
    exit 1
fi


n_server=2
n_simulation=3
n_runners=2


ensemble_size=9 # we need to use the same ensemble size as in the testcase!
total_steps=18  # TODO: I think totalsteps is not equal max_timestamp...

assimilator_type=0 # dummy
assimilator_type=1 # pdaf

######################################################

# trap ctrl-c and call ctrl_c()
trap ctrl_c INT

function ctrl_c() {
        echo "** Trapped CTRL-C"
        killall xterm
        killall $sim_exe
        killall $server_exe
        exit 0
}

precommand="xterm_gdb"
precommand=""
#precommand="xterm_gdb valgrind --leak-check=yes"
rm -rf output
mkdir -p output
cd output
rm -f nc.vg.*

rm -f *_ana.txt
rm -f *_for.txt

#precommand="xterm -e valgrind --track-origins=yes --leak-check=full --show-reachable=yes --log-file=nc.vg.%p"
#precommand="xterm -e valgrind --show-reachable=no --log-file=nc.vg.%p"
#precommand="xterm -e valgrind --vgdb=yes --vgdb-error=0 --leak-check=full --track-origins=yes --show-reachable=yes"
if [[ "$1" == "test" ]];
then
  # TODO: add ensemble size, max timesteps
  #total_steps=$2
  #ensemble_size=$3
  #n_server=$4
  #n_simulation=$5
  #n_runners=$6

  n_server=1
  n_simulation=1
  n_runners=1


  echo testing with $n_server server procs and $n_runners times $n_simulation simulation nodes.

  precommand=""
else
  echo TODO: please add manually the patch if existent!
  echo '(patch-PDAF....)'
  echo compiling....
  set -e
  cd ..
  ./compile.sh
  cd -
  set +e
fi

source ../build/install/bin/melissa-da_set_env.sh

sim_exe_path="$MELISSA_DA_PATH/bin/simulation2-pdaf"
server_exe_path="$MELISSA_DA_PATH/bin/melissa_server"


killall xterm
killall lorenz_96
killall example_simulation

srun bash -c 'killall simulation2-pdaf; killall melissa_server'
sleep 1


$MPIEXEC -n $n_server \
  $precommand $server_exe_path $total_steps $ensemble_size $assimilator_type &>ser.log &

sleep 1

export MELISSA_SERVER_MASTER_NODE="tcp://localhost:4000"

max_runner=`echo "$n_runners - 1" | bc`
for i in `seq 0 $max_runner`;
do
#  sleep 0.3  # use this and more than 100 time steps if you want to check for the start of propagation != 1... (having model task runners that join later...)
  #echo start simu id $i
  $MPIEXEC -n $n_simulation \
    $precommand $sim_exe_path &>sim.log &


  echo .
done


wait

