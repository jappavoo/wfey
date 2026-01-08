#!/bin/bash

#set -x

# --------- Run HWMON --------- #
HEADER="sensors -u altra_hwmon-isa-0000 | tail -n +5 | grep -o 'energy.*_input' | tr '\n' ' '; echo -e '\n'"
VALUES="sensors -u altra_hwmon-isa-0000 | tail -n +5 | grep -o '[0-9]*\.[0-9]*' | tr '\n' ' '; echo -e '\n'"

trap 'cleanup' 10 USR1

function powerLog() {
    LOGPATH=$1
    LOGDATE=$2
    
    eval "$HEADER" > $LOGPATH/hwmon-${LOGDATE}.out
    eval "$VALUES" >> $LOGPATH/hwmon-${LOGDATE}.out
    while [[ 1 ]]; do sleep 10; done
}

function cleanup() {
    eval "$VALUES" >> $LOGPATH/hwmon-${LOGDATE}.out
}

powerLog "$1" "$2"

