#!/bin/bash
#if echo $PWD | grep "api" || echo $PWD | grep pdaf-wrapper || echo $PWD | grep PDAF ;
if echo $PWD | grep "api"  || echo $PWD | grep PDAF ;
then
    echo ${1} ${@:2}
    ${1} ${@:2}
else
    scorep --compiler ${1}  "${@:2}"
    #scorep --compiler --instrument-filter="$HOME/workspace/melissa-da/profiling/filter_scorep"  ${1}  "${@:2}"
fi
