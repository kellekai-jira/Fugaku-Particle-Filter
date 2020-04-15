#!/bin/bash
if echo $PWD | grep "api" || echo $PWD | grep pdaf-wrapper || echo $PWD | grep PDAF ;
then
    echo mpi${1} ${@:2}
    mpi${1} ${@:2}
else
    scorep --compiler mpi${1}  "${@:2}"
    #scorep --pdt mpi${1}  "${@:2}"
fi
