#!/bin/bash
#set -x
# --------- Run PDU Script --------- #

PDU_LEFT=0
PDU_RIGHT=1
DON=0
BEAST=1

function powerLog() {
    LOCALPATH=$1
    LOCALDATE=$2
    echo "PDU_LEFT BEAST" > $LOCALPATH/pdu-left-beast-${LOCALDATE}.out
    # Note: don't know if there is a difference between left and right yet so testing out both right now
    echo "PDU_RIGHT BEAST" > $LOCALPATH/pdu-right-beast-${LOCALDATE}.out
    while sleep 0.5; do
	${SCRIPTDIR}/pdu/getPDUInfo.sh 1 $PDU_LEFT $BEAST 0 0 >> $LOCALPATH/pdu-left-beast-${LOCALDATE}.out
	${SCRIPTDIR}/pdu/getPDUInfo.sh 1 $PDU_RIGHT $BEAST 0 0 >> $LOCALPATH/pdu-right-beast-${LOCALDATE}.out
    done
}

#powerLog "$1" "$2"
