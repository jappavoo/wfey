#!/bin/bash

# --------- Run HWMON --------- #
HEADER="sensors altra_hwmon-isa-0000 | tail -n +4 | cut -d ':' -f 1 | tr '\n' ' '"
VALUES="sensors altra_hwmon-isa-0000 | tail -n +4 | cut -d ' ' -f 5,6 | tr '\n' ' '"

function powerLog() {
    LOGPATH=$1
    LOGDATE=$2
    #echo "hwmon0" > $LOGPATH/hwmon-${LOGDATE}.out
    eval "$HEADER" > $LOGPATH/hwmon-${LOGDATE}.out
    while sleep 0.5; do
	#echo $(cat /sys/class/hwmon/hwmon0/power1_input) >> $LOGPATH/hwmon-${LOGDATE}.out
	eval "$VALUES" >> $LOGPATH/hwmon-${LOGDATE}.out
    done
}

# gets the core values & units
# sensors altra_hwmon-isa-0000 | tail -n +4 | head -n -1 | cut -d ' ' -f 5,6

# gets the header
# sensors altra_hwmon-isa-0000 | tail -n +4 | head -n -1 | cut -d ':' -f 1

