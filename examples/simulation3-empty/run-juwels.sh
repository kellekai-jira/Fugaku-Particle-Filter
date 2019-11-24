#!/bin/bash -x


# this is the testcase to see if we scale very high???


n_server=96
n_simulation=96
n_runners=1

nodes_simulation=2

ensemble_size=700
ensemble_size=1
total_steps=5
total_steps=50

####################
# not overwritten in test:
problem_size=10000000
#--> MPI_Bsend(184).......: MPI_Bsend(buf=0x7ffd958c7f60, count=2, MPI_INT, dest=0, tag=43, MPI_COMM_WORLD) failed
#MPIR_Bsend_isend(311): Insufficient space in Bsend buffer; requested 8; total buffer size is 0Fatal error in MPI_Bsend: Invalid buffer pointer, error stack

problem_size=1000000
problem_size=100000
# still....


# works:
problem_size=1000
#problem_size=10000000

problem_size=10000000
# worked once... error seems not related to this...

# in s
max_runner_timeout=120

CORES_PER_SIMULATION_NODE=48
CORES_PER_SERVER_NODE=4

######################################################

# trap ctrl-c and call ctrl_c()
#trap ctrl_c INT


function kill_cmd() {
    srun bash -c "killall $sim_exe; killall $server_exe; killall xterm"
}

function ctrl_c() {
        echo "** Trapped CTRL-C"
        kill_cmd
        exit 0
}

precommand=""

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


nodes_server=$((n_server / CORES_PER_SERVER_NODE))
nodes_simulation=$((n_simulation / CORES_PER_SIMULATION_NODE))

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
sim_exe="simulation3-empty"

kill_cmd

server_exe_path="$bin_path/$server_exe"
sim_exe_path="$bin_path/$sim_exe"


rm output.txt

rm server.log.*

# todo: catch error if not enough nodes are available!
# what happens when each simulation uses less than one node?


# pinning - thats where it gets a bit complicated... unfortunately....
# get all hosts:
nodelist=`srun hostname | cut -d '.' -f 1`
nodelist=`echo $nodelist | sed -e 's/ /,/g'`
nodelist_pointer=1


nodelist_server=`echo $nodelist | cut -d ',' -f${nodelist_pointer}-$((nodelist_pointer+nodes_server-1))`
nodelist_pointer=$((nodelist_pointer+nodes_server))

#$MPIEXEC -N $nodes_server -n $n_server --nodelist=$nodelist_server $precommand \
#  /bin/bash -c "$precommand $server_exe_path $total_steps $ensemble_size 2 $max_runner_timeout > server.log.\$\$" &

#export TRACENAME=melissa_server_${n_server}p.prv
#precommand=$HOME/workspace/melissa-da/extrae/trace.sh
precommand=""
export SCOREP_ENABLE_TRACING=false
export SCOREP_ENABLE_PROFILING=true
#export SCOREP_ENABLE_UNWINDING=true
export SCOREP_TOTAL_MEMORY=300M
export SCOREP_SAMPLING_EVENTS=perf_cycles@2000000
export SCAN_ANALYZE_OPTS="--time-correct"
#export SCOREP_ENABLE_TRACING=true
#scalasca -analyze
#scan
scan $MPIEXEC -N $nodes_server -n $n_server --ntasks-per-node=$CORES_PER_SERVER_NODE --nodelist=$nodelist_server $precommand \
  $precommand $server_exe_path $total_steps $ensemble_size 2 $max_runner_timeout > server.log.all &


sleep 1

export SCOREP_ENABLE_TRACING=false
export SCOREP_ENABLE_PROFILING=false
export SCOREP_ENABLE_UNWINDING=false

max_runner=`echo "$n_runners - 1" | bc`
for i in `seq 0 $max_runner`;
do
#  sleep 0.3  # use this and more than 100 time steps if you want to check for the start of propagation != 1... (having model task runners that join later...)
  echo start  $i
    nodelist_simulation=`echo $nodelist | cut -d ',' -f${nodelist_pointer}-$((nodelist_pointer+nodes_simulation-1))`
    nodelist_pointer=$((nodelist_pointer+nodes_simulation))
    $MPIEXEC -N $nodes_simulation -n $n_simulation --ntasks-per-node=$CORES_PER_SIMULATION_NODE --nodelist=$nodelist_simulation $precommand $sim_exe_path $problem_size > sim.log.$i&
  echo .
done

wait


server_rank_0_log=`ls server.log.* | head -n 1`
echo Server rank 0  log file: $server_rank_0_log

sed -n '/Run information/,/End Run information/p' $server_rank_0_log | sed -e '1,2 d; $ d' >> output.csv
