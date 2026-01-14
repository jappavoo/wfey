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

if [ ! -f $WFEYCONFIG ]; then make; fi

ARGSTR=${PARAMETERS// /_}

#LOGDATE=$(date +%Y-%m-%d-%H-%M-%S)

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


if [[ -z "$BM_ENERGY" ]]; then
    # --------- Run ENERGY SCRIPT --------- #
    ## This script should contain a function called 'powerLog' which should
    ## take in the Path and the Data args to create a file '$LOGPATH/hwmon-${LOGDATE}.out'

    ## NOTE -- I believe due to inheritance, energyscript should also be
    ## taskset to the runnercpu core but I have not confirmed this yet
    source $ENERGYSCRIPT

    powerLog "$LOGPATH" "$LOGDATE" &
    powerPID=$!
fi

# ---------- Run WFEY ---------- #

./$@ 1> $LOGPATH/latency-${LOGDATE}.out 2> $LOGPATH/wfey-${LOGDATE}.out

if [[ -z "$BM_ENERGY" ]]; then
    sleep ${SLEEP_TO_FINISH} # Power numbers from hwmon continue to go up a little after the wfey code is done

    echo "energy type is" $ENERGYTYPE
    
    if [[ "$ENERGYTYPE" == "NOMOD" ]]; then
	echo "nomod"
	kill -USR1 $powerPID
    elif [[ "$ENERGYTYPE" == "WMOD" ]]; then
	echo "wmod"
	cleanup "$LOGPATH" "$LOGDATE"
    fi
fi
