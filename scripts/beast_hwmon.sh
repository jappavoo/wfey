#!/bin/bash

# --------- Run HWMON --------- #

function powerLog() {
    LOCALPATH=$1
    LOCALDATE=$2
    echo "hwmon0" > $LOCALPATH/hwmon-${LOCALDATE}.out
    while sleep 0.5; do
	echo $(cat /sys/class/hwmon/hwmon0/power1_input) >> $LOCALPATH/hwmon-${LOCALDATE}.out
    done
}

powerLog "$1" "$2"
