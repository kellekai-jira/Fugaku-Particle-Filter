#!/bin/bash


set -e
cd $HOME/workspace/melissa-da
./compileall.sh
cd -

source $HOME/workspace/melissa-da/build/install/bin/melissa-da_set_env.sh
set +e

function cleanup() {
    echo "cleaning up old processes..."
    killall xterm
    killall melissa_server
    killall `basename $MPIEXEC`
    killall gdb
}

cleanup


function ctrl_c() {
        echo "** Trapped CTRL-C"
        cleanup
        exit 1
}

trap ctrl_c INT

python script.py
