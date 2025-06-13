#!/bin/bash

hwmon_num=$1

if [[ -z "$hwmon_num" ]]; then
   echo "Enter HW monitor number that checks for CPU power"
   exit -1
fi

while sleep 0.5; do cat /sys/class/hwmon/hwmon${hwmon_num}/power1_input; done
