#!/bin/bash

#set -x

# This script should be run with "taskset -c 80"
source ./scripts/env.sh

for config in "${wfeyconfig[@]}"; do
    for events in "${numevents[@]}"; do
	for sleeping in "${sleeptime[@]}"; do
	    for eventproc in "${eventprocCPUs[@]}"; do
		for sources in "${sourceCPUs[@]}"; do
		    echo "./scripts/run_allegro.sh $config $events $eventproc $sleeping $sources"
		    taskset -c ${runnercpu} ./scripts/run_allegro.sh $config $events $eventproc $sleeping $sources
		done
	    done
	done
    done
done
