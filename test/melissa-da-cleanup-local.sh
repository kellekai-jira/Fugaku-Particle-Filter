#!/bin/bash
for run in 1 2
do
    for procname in melissa_server simulation1 simulation1-deadlock simulation1-hidden simulation1-hidden-index-map simulation1-index-map simulation1-stateful simulation2-pdaf simulation3-empty
    do
        while pgrep $procname;
        do
            echo found some $procname to kill...
            pkill $procname
            sleep 0.1
        done
    done
done
