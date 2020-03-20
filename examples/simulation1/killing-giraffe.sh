#!/bin/bash
# this script will randomly kill 1 procs. we will show that the test case still does not
# break.

proc_name=$1

procs=`ps ax | grep $proc_name | grep -v grep | sed -e 's/^[ ]*//g' | cut -d' ' -f 1`
lines=`echo "$procs" | wc -l`
killid=$(($RANDOM % $lines + 1))
pid=`echo "$procs" | cut -d$'\n' -f$killid`

#echo "$procs"

echo killing proc $killid / $lines with pid $pid

kill -9 $pid


