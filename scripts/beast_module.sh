#!/bin/bash

# --------- Run HWMON --------- #
HEADER="sensors -u altra_hwmon-isa-0000 | tail -n +5 | grep -o 'energy.*_input' | tr '\n' ' '; echo -e '\n'"
VALUES="sensors -u altra_hwmon-isa-0000 | tail -n +5 | grep -o '[0-9]*\.[0-9]*' | tr '\n' ' '; echo -e '\n'"

function powerLog() {
    LOGPATH=$1
    LOGDATE=$2
    eval "$HEADER" > $LOGPATH/hwmon-${LOGDATE}.out
    while sleep 0.5; do
	eval "$VALUES" >> $LOGPATH/hwmon-${LOGDATE}.out
    done
}
