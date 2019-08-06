#!/bin/bash
simuid=$1

precommand="xterm_gdb"

n_simulation=2

sim_exe="/home/friese/workspace/melissa-da/build_example-simulation/example_simulation"
lib_paths="/home/friese/workspace/melissa-da/build_api:/home/friese/workspace/melissa/install/lib"

mpirun -n $n_simulation \
  -x MELISSA_SERVER_MASTER_NODE="tcp://narrenkappe:4000" \
  -x LD_LIBRARY_PATH=$lib_paths \
  $precommand $sim_exe &

