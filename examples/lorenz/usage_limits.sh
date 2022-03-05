#!/bin/bash
paste <(grep 'open files\|processes\|pending signals' \
              /proc/self/limits | cut -c27-38)        \
      <(i=`whoami`
        lsof -n -u $i 2> /dev/null | 
             tail -n +2 | 
             awk {'print $9'} | 
             wc -l
        ps --no-headers -U $i -u $i u | wc -l
        ps -u $i -o pid= | 
             xargs printf "/proc/%s/status\n" |
             xargs grep -s 'SigPnd' |
             sed 's/.*\t//' | 
             paste -sd+ | 
             bc ; ) \
      <(grep 'open files\|processes\|pending signals' \
             /proc/self/limits | 
                   cut -c1-19) | 
while read a b name ; do 
    printf '%3i%%  %s\n' $((${b}00/a)) "$name"
done
