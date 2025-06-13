#!/bin/bash

set -m # enables job control

#VERBOSE=1

SCRIPTDIR=$(dirname $0)
WFEYCONFIG=$1

PARAMETERS=$(echo "${*:2}")

ARGSTR=${PARAMETERS// /_}

LOGPATH="logs/$WFEYCONFIG/$ARGSTR"

LOGDATE=$(date +%Y-%m-%d-%H-%M-%S)

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


$SCRIPTDIR/minmax_cpu_power.sh 2 1 &> $LOGPATH/hwmon2-${LOGDATE}.out &
$SCRIPTDIR/minmax_cpu_power.sh 3 1 &> $LOGPATH/hwmon3-${LOGDATE}.out &

./$@ &> $LOGPATH/wfey-${LOGDATE}.out

sleep 3

$SCRIPTDIR/minmax_kill.sh

# Get Min
# tail -n hwmon<n>*.out | cut -d ':' -f 2 | cut -d ' ' -f 1

# Get Max
# tail -n hwmon<n>*.out | cut -d ':' -f 3

# Get Diff
# echo $max-$min | bc
