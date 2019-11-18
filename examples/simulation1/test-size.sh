#!/bin/bash -x

# this is the testcase to see if we scale very high???


n_server=96
n_simulation=96
n_runners=1

nodes_server=2
nodes_simulation=2

ensemble_size=700
ensemble_size=1
total_steps=5
total_steps=50



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

rm sim.log.*
precommand="xterm_gdb"
precommand=""
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

# for srun:
# do not put more than one rank per core...
#export MPIEXEC="$MPIEXEC --exclusive --ntasks-per-node=48 "
export MPIEXEC="$MPIEXEC"
#--hint=nomultithread


#get hostnames with this simple trick (assuming wir are in a job allocation ;)
server_host_0=`srun -N 1 -n 1 hostname`
export MELISSA_SERVER_MASTER_NODE="tcp://$server_host_0:4000" 
rm $tmpfile

bin_path="$MELISSA_DA_PATH/bin"

server_exe="melissa_server"
sim_exe="simulation1"

killall xterm
killall $server_exe
killall $sim_exe

srun bash -c 'killall simulation1; killall melissa_server'

server_exe_path="$bin_path/$server_exe"
sim_exe_path="$bin_path/$sim_exe"


rm output.txt

rm server.log.*
# get all hosts:
nodelist=`srun hostname | cut -d '.' -f 1`
nodelist=`echo $nodelist | sed -e 's/ /,/g'`
nodelist_pointer=1

nodelist_server=`echo $nodelist | cut -d ',' -f${nodelist_pointer}-$((nodelist_pointer+nodes_server-1))`
nodelist_pointer=$((nodelist_pointer+nodes_server))

$MPIEXEC -N $nodes_server -n $n_server --nodelist=$nodelist_server $precommand $server_exe_path $total_steps $ensemble_size > server.log.$$ &


sleep 1

max_runner=`echo "$n_runners - 1" | bc`
for i in `seq 0 $max_runner`;
do
#  sleep 0.3  # use this and more than 100 time steps if you want to check for the start of propagation != 1... (having model task runners that join later...)
  echo start  $i
    nodelist_simulation=`echo $nodelist | cut -d ',' -f${nodelist_pointer}-$((nodelist_pointer+nodes_simulation-1))`
    nodelist_pointer=$((nodelist_pointer+nodes_server))
    $MPIEXEC -N $nodes_simulation -n $n_simulation --nodelist=$nodelist_simulation $precommand $sim_exe_path > sim.log.$i&
  echo .
done


wait


./plot_size.sh
