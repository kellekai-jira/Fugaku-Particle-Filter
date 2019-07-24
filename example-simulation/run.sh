#!/bin/bash

killall xterm


for i in `seq 0 3`;
do
  echo start simu id $i
  mpirun -n 2 -x MELISSA_SIMU_ID=$i -x MELISSA_SERVER_MASTER_NODE="tcp://narrenkappe:4000" -x LD_LIBRARY_PATH=/home/friese/workspace/melissa-da/build_api:/home/friese/workspace/melissa/install/lib xterm_gdb /home/friese/workspace/melissa-da/build_example-simulation/example_simulation &
done

wait
