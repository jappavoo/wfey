#!/bin/bash

HWMON_No=$1
VERBOSE=$2

if [[ -z $HWMON_No ]]; then
    echo "USAGE: $0 <hardware monitor number>"
    exit -1
fi

min=1000000000
max=0


function cleanup()
{
    echo "HWMON_No${HWMON_No} = min:${min}  max:${max}"
    exit
}

trap cleanup SIGINT


while sleep 0.5; do
    a=$(cat /sys/class/hwmon/hwmon${HWMON_No}/power1_input);

    [[ -n $VERBOSE ]] && echo $a
    
    min=$(( a < min ? a : min ))
    max=$(( a > max ? a : max ))
done


