#!/bin/bash

# ----------- SetUp ------------ #

source ./scripts/env.sh
set -m # enables job control
#set -x 
#VERBOSE=1

SCRIPTDIR=$(dirname $0)
WFEYCONFIG=$1
PARAMETERS=$(echo "${*:2}")

if [[ -z "$WFEYCONFIG" || -z "$PARAMETERS" ]]; then echo "USAGE($0): Input parameters as you would to wfey benchmark"; exit -1; fi
# Should be checking parameter length ... 


ARGSTR=${PARAMETERS// /_}

LOGDATE=$(date +%Y-%m-%d-%H-%M-%S)

LOGPATH="logs/$LOGS/$WFEYCONFIG/$ARGSTR/"


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


# --------- Run ENERGY SCRIPT --------- #
## This script should contain a function called 'powerLog' which should
## take in the Path and the Data args to create a file '$LOGPATH/hwmon-${LOGDATE}.out'

## NOTE -- I believe due to inheritance, energyscript should also be
## taskset to the runnercpu core but I have not confirmed this yet
bash $ENERGYSCRIPT "$LOGPATH" "$LOGDATE" &
powerPID=$!

# ---------- Run WFEY ---------- #

./$@ 1> $LOGPATH/latency-${LOGDATE}.out 2> $LOGPATH/wfey-${LOGDATE}.out

sleep ${SLEEP_TO_FINISH} # Power numbers from hwmon continue to go up a little after the wfey code is done

kill -USR1 $powerPID
