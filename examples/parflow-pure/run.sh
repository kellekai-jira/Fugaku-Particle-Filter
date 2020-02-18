#!/bin/bash


n_server=2      # TODO: make this changeable
n_simulation=2  # TODO this is fixed so far and may not be changed
n_runners=4
n_runners=1

ensemble_size=3
ensemble_size=1
ensemble_size=9
total_steps=18  # TODO: I think totalsteps is not equal max_timestamp...
total_steps=180  # TODO: I think totalsteps is not equal max_timestamp...

assimilator_type=0 # dummy
assimilator_type=1 # pdaf

timeout=30

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
#precommand=""




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


  echo testing with $n_server server procs and $n_runners times $n_simulation simulation nodes.

  precommand=""
else
  echo compiling melissa-da....
  set -e
  cd ../..
  ./compile.sh
  cd -
  set +e
fi

source ../../build/install/bin/melissa-da_set_env.sh


# now compile parflow

if [[ "$1" == "test" ]];
then
  # TODO: add ensemble size, max timesteps
  #total_steps=$2
  #ensemble_size=$3
  #n_server=$4
  #n_simulation=$5
  #n_runners=$6


  echo testing with $n_server server procs and $n_runners times $n_simulation simulation nodes.

  precommand=""
else
  echo compiling parflow....
  set -e
  cd $MELISSA_DA_PATH/../../examples/parflow-pure/parflow-melissa-da
  ./compile.sh
  cd -
  set +e
fi

export PARFLOW_DIR=$MELISSA_DA_PATH/../../examples/parflow-pure/parflow-melissa-da/build/install


#precommand="xterm_gdb valgrind --leak-check=yes"
#rm -rf output
#mkdir -p output
#cd output
#rm -f nc.vg.*

sim_exe=parflow
server_exe=melissa_server
sim_exe_path="$PARFLOW_DIR/bin/$sim_exe"
server_exe_path="$MELISSA_DA_PATH/bin/$server_exe"


killall xterm
killall $sim_exe
killall $server_exe

set -e
echo pf-dir: $PARFLOW_DIR
exec_dir=$MELISSA_DA_PATH/../../examples/parflow-pure/parflow-melissa-da/test/2
cd $exec_dir

# create simulatioin input:
tclsh pfin.tcl
set +e
# TODO: check if works with preload!
preload="$MELISSA_DA_PATH/lib/libmelissa_pdaf_wrapper_parflow_pure.so"
PRELOAD_VAR="LD_PRELOAD"
if [ ! -z "$precommand" ];
then
    PRELOAD_VAR="XTERM_GDB_LD_PRELOAD"
fi
$MPIEXEC -n $n_server \
  -x LD_LIBRARY_PATH=$LD_LIBRARY_PATH \
  -x $PRELOAD_VAR="$preload" \
  $precommand $server_exe_path $total_steps $ensemble_size $assimilator_type $timeout &> server.out &


sleep 1


rm -rf runner_*/
max_runner=`echo "$n_runners - 1" | bc`
for i in `seq 0 $max_runner`;
do
#  sleep 0.3  # use this and more than 100 time steps if you want to check for the start of propagation != 1... (having model task runners that join later...)
  #echo start simu id $i
  runner_dir="runner_$i"
  mkdir -p $runner_dir
  cp pfin.pfidb $runner_dir/
  cd $runner_dir
  $MPIEXEC -n $n_simulation \
    -x MELISSA_SERVER_MASTER_NODE="tcp://localhost:4000" \
    -x LD_LIBRARY_PATH=$LD_LIBRARY_PATH \
    $precommand $sim_exe_path pfin &> simulation.out &
  cd $exec_dir


  echo .
done


wait

# Do some post processing:

# Generate csv files
postfix=".${n_runners}runners.${ensemble_size}members"
sed -n -e '/^------------------- Timing information(csv): -------------------$/,/^------------------- End Timing information -------------------$/{ /^------------------- Timing information(csv): -------------------$/d; /^------------------- End Timing information -------------------$/d; p; }' server.out > server-all.csv$postfix
sed -n -e '/^------------------- Run information(csv): -------------------$/,/^------------------- End Run information -------------------$/{ /^------------------- Run information(csv): -------------------$/d; /^------------------- End Run information -------------------$/d; p; }' server.out > server-short.csv$postfix

sed -n -e '/^------------------- Timing information(csv): -------------------$/,/^------------------- End Timing information -------------------$/{ /^------------------- Timing information(csv): -------------------$/d; /^------------------- End Timing information -------------------$/d; p; }' runner_0/simulation.out > simulation-all.csv$postfix
sed -n -e '/^------------------- Run information(csv): -------------------$/,/^------------------- End Run information -------------------$/{ /^------------------- Run information(csv): -------------------$/d; /^------------------- End Run information -------------------$/d; p; }' runner_0/simulation.out > simulation-short.csv$postfix

# generate nc files
cd runner_0
ls *.press.*.pfb | xargs ~/workspace/PFBtoNC/PFBtoNCVCool
ls *.satur.*.pfb | xargs ~/workspace/PFBtoNC/PFBtoNCVCool
ls *.dens.*.pfb | xargs ~/workspace/PFBtoNC/PFBtoNCVCool



