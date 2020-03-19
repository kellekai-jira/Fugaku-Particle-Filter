#!/bin/bash
if [ "$#" -ne 1 ]; then
    echo "Illegal number of parameters"
    echo "usage: $0 <path to simulation executable linked against melissa>"
    exit 1
fi

source ../build/install/bin/melissa-da_set_env.sh

bin_path="$MELISSA_DA_PATH/bin"

server_exe="melissa_server"
sim_exe_path="$1"

killall xterm
killall $server_exe
killall `basename $sim_exe_path`

server_exe_path="$bin_path/$server_exe"

precommand=""

rm output.txt

MPIEXEC=mpiexec
log=`tempfile`
echo logging to $log
$MPIEXEC -n 1 \
  -x LD_LIBRARY_PATH=$LD_LIBRARY_PATH \
  $precommand $server_exe_path 3 1 3 10 &> $log &  # start server in stateless check mode
sleep 1

$MPIEXEC -n 1 \
  -x MELISSA_SERVER_MASTER_NODE="tcp://localhost:4000" \
  -x LD_LIBRARY_PATH=$LD_LIBRARY_PATH \
  $precommand $sim_exe_path &>/dev/null &


echo .


wait
echo Successful, Simulation $1 stateless?
if grep '**** Check Successful' $log &> /dev/null
then
  echo yes
  rm $log
  exit 0
else
  echo no
  rm $log
  exit 1
fi
