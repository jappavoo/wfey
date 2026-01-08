#!/bin/bash

# --------- Run HWMON --------- #

function powerLog() {
    LOGPATH=$1
    LOGDATE=$2
    echo "hwmon0" > $LOGPATH/hwmon-${LOGDATE}.out
    while sleep 0.5; do
	echo $(cat /sys/class/hwmon/hwmon0/power1_input) >> $LOGPATH/hwmon-${LOGDATE}.out
    done
}

powerLog "$1" "$2"
