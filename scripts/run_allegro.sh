#!/bin/bash

# ----------- SetUp ------------ #

set -m # enables job control
#set -x 
#VERBOSE=1

SCRIPTDIR=$(dirname $0)
WFEYCONFIG=$1

PARAMETERS=$(echo "${*:2}")

ARGSTR=${PARAMETERS// /_}

LOGDATE=$(date +%Y-%m-%d-%H-%M-%S)

LOGPATH="logs/$WFEYCONFIG/$ARGSTR/$LOGPATH"


if [[ -n $VERBOSE ]]; then
    echo "SCRIPTDIR  = $SCRIPTDIR"
    echo "WFEYCONFIG = $WFEYCONFIG"
    echo "PARAMETERS = $PARAMETERS"
    echo "ARG STRING = $ARGSTR"
    echo "LOGPATH    = $LOGPATH"
    echo "LOGDATE    = $LOGDATE"
fi


[[ -n $VERBOSE ]] && echo "mkdir -p ${LOGPATH}"
mkdir -p $LOGPATH


# --------- Run HWMON --------- #

powerLog() {
    echo "hwmon2 hwmon3" > $LOGPATH/hwmon-${LOGDATE}.out
    while sleep 0.5; do
	echo $(cat /sys/class/hwmon/hwmon2/power1_input /sys/class/hwmon/hwmon3/power1_input) >> $LOGPATH/hwmon-${LOGDATE}.out
    done
}

powerLog &
powerPID=$!

# ---------- Run WFEY ---------- #

./$@ 1> $LOGPATH/latency-${LOGDATE}.out 2> $LOGPATH/wfey-${LOGDATE}.out

sleep 3 # Power numbers continue to go up a little after the wfey code is done

kill -15 $powerPID



#$SCRIPTDIR/minmax_cpu_power.sh 2 1 &> $LOGPATH/hwmon2-${LOGDATE}.out &
#$SCRIPTDIR/minmax_cpu_power.sh 3 1 &> $LOGPATH/hwmon3-${LOGDATE}.out &

#$SCRIPTDIR/minmax_kill.sh

# Get Min
# tail -n hwmon<n>*.out | cut -d ':' -f 2 | cut -d ' ' -f 1
# cat hwmon-*.out | cut -f 1 -d ' ' | tail -n +2 | sort | head -1

# Get Max
# tail -n hwmon<n>*.out | cut -d ':' -f 3
# # cat hwmon-*.out | cut -f 1 -d ' ' | tail -n +2 | sort -r | head -1

# Get Diff
# echo $max-$min | bc
