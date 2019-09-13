#!/bin/bash
# REM: only works if simulation timeout is set to 5 seconds on the serverside. otherwise
#      the timing here won't work!

cd ..
./compile.sh
cd -

start_time=`date +%s`

./run.sh test 200 42 3 2 10 &
sleep 2
./killing-giraffe.sh example_simulation
sleep 7
./killing-giraffe.sh example_simulation

wait


./compare.sh reference-giraffe.txt
result=$?

end_time=`date +%s`

runtime=$((end_time-start_time))
echo this took $runtime seconds!
exit $result
