#!/bin/bash

#set -x

# --------- Run HWMON --------- #
HEADER="sensors -u altra_hwmon-isa-0000 | tail -n +5 | grep -o 'energy.*_input' | tr '\n' ' '; echo -e '\n'"
VALUES="sensors -u altra_hwmon-isa-0000 | tail -n +5 | grep -o '[0-9]*\.[0-9]*' | tr '\n' ' '; echo -e '\n'"

#trap 'cleanup' 10 USR1

function powerLog() {
    LOCALPATH=$1
    LOCALDATE=$2
    
    eval "$HEADER" > $LOCALPATH/hwmon-${LOCALDATE}.out
    eval "$VALUES" >> $LOCALPATH/hwmon-${LOCALDATE}.out
    #while [[ 1 ]]; do sleep 10; done
}

function cleanup() {
    LOCALPATH=$1
    LOCALDATE=$2
    
    eval "$VALUES" >> $LOCALPATH/hwmon-${LOCALDATE}.out
}

#powerLog "$1" "$2"

