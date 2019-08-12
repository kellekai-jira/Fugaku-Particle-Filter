#!/bin/bash

../compile.sh

start_time=`date +%s`

./run.sh test 200 42 3 2 10 &
sleep 2
./killing-giraffe.sh example_simulation
sleep 7
./killing-giraffe.sh example_simulation

wait


./compare.sh reference-giraffe.txt

end_time=`date +%s`

runtime=$((end_time-start_time))
echo this took $runtime seconds!
