#!/bin/bash

#set -x

FILEPATH=${1:-'/sys/class/hwmon/hwmon0/power1_input'}
OUTPUTFILE=${2:-'catmon.out'}
WITHSLEEP=${3:-0}

PERIOD=10 # Increasing the load every 10 sec
NUMCORES=$(nproc --all)

TIMETORUN=$((NUMCORES * PERIOD))


function killCore () {
    coreID=$(($1+1)) ## affinaity mask must be 1 indexed
    for pid in $(pgrep -f "./catmon_sleep.sh"); do
	if taskset -p $pid | grep -q "mask: [$coreID]"; then
            kill $pid
	fi
    done
}

function sleepCores () {
    for ((coreID=1 ; coreID < $NUMCORES ; coreID++ )); do
	taskset -c $coreID ./catmon_sleep.sh $PERIOD &
    done
}

[ $WITHSLEEP -eq 1 ] && {
    sleepCores
    sleep 1
} 

taskset -c 0 ./catmon $FILEPATH $TIMETORUN > $OUTPUTFILE &

sleep $PERIOD # all cores sleeping, expect runner core

# start to add a core doing hardwork every PERIOD seconds
for (( i=1 ; i<$NUMCORES ; i++ )); do
    [ $WITHSLEEP -eq 1 ] && killCore $i
    taskset -c $i ./catmon_work.sh &
    sleep $PERIOD
done

killall sha1sum

#sed -i '/^[ \t]*$/d' "$OUTPUTFILE"
