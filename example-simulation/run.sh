#!/bin/bash

killall xterm

precommand="xterm_gdb"

if [[ "$1" == "test" ]];
then
  precommand=""
fi



for i in `seq 0 1`;
do
  echo start simu id $i
  mpirun -n 2 -x MELISSA_SIMU_ID=$i -x MELISSA_SERVER_MASTER_NODE="tcp://narrenkappe:4000" -x LD_LIBRARY_PATH=/home/friese/workspace/melissa-da/build_api:/home/friese/workspace/melissa/install/lib $precommand /home/friese/workspace/melissa-da/build_example-simulation/example_simulation &

done

# trap ctrl-c and call ctrl_c()
trap ctrl_c INT

function ctrl_c() {
        echo "** Trapped CTRL-C"
        killall xterm
        exit 0
}

wait
