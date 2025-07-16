#!/bin/bash

#set -x

# This script should be run with "taskset -c 80"
source ./scripts/env.sh
times_to_run=20

for ((i = 0 ; i < ${times_to_run} ; i++ )); do
    for config in "${wfeyconfig[@]}"; do
	for events in "${numevents[@]}"; do
	    for sleeping in "${sleeptime[@]}"; do
		for eventproc in "${eventprocCPUs[@]}"; do
		    for sources in "${sourceCPUs[@]}"; do
			echo "./scripts/run_allegro.sh $config $events $eventproc $sleeping $sources"
			taskset -c ${runnercpu} ./scripts/run_allegro.sh $config $events $eventproc $sleeping $sources
			sleep 5 # Should wait for the power numbers to go down before the next run
		    done
		done
	    done
	done
    done
done
