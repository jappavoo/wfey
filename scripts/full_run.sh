#!/bin/bash

## NOTE
# This script should be run with "taskset -c $runnercpu"

#set -x

( set -o posix; set ) > /tmp/var.before
source ./scripts/env.sh
( set -o posix; set ) > /tmp/var.after

TESTDATE=$(date +%Y-%m-%d-%H-%M-%S)
mkdir -p $PRETESTPATH
mkdir -p ${PRETESTPATH}/${TESTDATE}

diff /tmp/var.before /tmp/var.after > $PRETESTPATH/${TESTDATE}/env.out

for ((i = 0 ; i < ${times_to_run} ; i++ )); do
    export LOGDATE=$(date +%Y-%m-%d-%H-%M-%S)
    cat /proc/loadavg > $PRETESTPATH/${TESTDATE}/loadavg-${LOGDATE}.out
    for config in "${wfeyconfig[@]}"; do
	for events in "${eventrate[@]}"; do
	    for eventproc in "${eventprocCPUs[@]}"; do
		for sources in "${sourceCPUs[@]}"; do
		    echo "$i: ./scripts/run_wfey.sh $config $events $eventproc $sources"
		    taskset -c ${runnercpu} ./scripts/run_wfey.sh $config $events $eventproc $sources
		    sleep ${SLEEP_TO_RESET} # Should wait for the power numbers to go down before the next run
		done
	    done
	done
    done
done
