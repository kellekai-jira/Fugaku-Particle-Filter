#!/bin/bash

killall xterm

precommand="xterm_gdb"
#precommand="xterm_gdb valgrind --leak-check=yes"
rm nc.vg.*
#precommand="xterm -e valgrind --track-origins=yes --leak-check=full --show-reachable=yes --log-file=nc.vg.%p"
#precommand="xterm -e valgrind --show-reachable=no --log-file=nc.vg.%p"
#precommand="xterm -e valgrind --vgdb=yes --vgdb-error=0 --leak-check=full --track-origins=yes --show-reachable=yes"

if [[ "$1" == "test" ]];
then
  precommand=""
fi

sleep 3

LD_LIBRARY_PATH=/home/friese/workspace/melissa-da/build_api:/home/friese/workspace/melissa/install/lib xterm_gdb /home/friese/workspace/melissa-da/build_server/melissa_server &

sleep 3

for i in `seq 0 0`;
do
  echo start simu id $i
  mpirun -n 4 -x MELISSA_SIMU_ID=$i -x MELISSA_SERVER_MASTER_NODE="tcp://narrenkappe:4000" -x LD_LIBRARY_PATH=/home/friese/workspace/melissa-da/build_api:/home/friese/workspace/melissa/install/lib $precommand /home/friese/workspace/melissa-da/build_example-simulation/example_simulation &
#LD_PRELOAD=/usr/lib/valgrind/libmpiwrap-amd64-linux.so mpirun -n 4 -x MELISSA_SIMU_ID=$i -x MELISSA_SERVER_MASTER_NODE="tcp://narrenkappe:4000" -x LD_LIBRARY_PATH=/home/friese/workspace/melissa-da/build_api:/home/friese/workspace/melissa/install/lib $precommand /home/friese/workspace/melissa-da/build_example-simulation/example_simulation &

done

# trap ctrl-c and call ctrl_c()
trap ctrl_c INT

function ctrl_c() {
        echo "** Trapped CTRL-C"
        killall xterm
        exit 0
}

wait
