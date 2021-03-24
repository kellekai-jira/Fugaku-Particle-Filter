#!/bin/bash

# can be removed only once. all the others will throw an error...
if rm "$BE_NEVER_CONNECTING";
then
    echo "I'm the never connecting runner"
    echo "pid=$$"
    echo "I'm the never connecting runner with id $MELISSA_DA_RUNNER_ID" > never_connecting_runner
    echo "pid=$$" >> never_connecting_runner

    while true;
    do
        sleep 1
    done
else
    exec simulation1
fi
