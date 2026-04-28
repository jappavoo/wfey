#!/bin/bash

# --------- Run PDU Script --------- #

PDU_LEFT=0
PDU_RIGHT=1
DON=0
BEAST=1


function powerLog() {
    LOGPATH=$1
    LOGDATE=$2
    echo "PDU_LEFT BEAST" > $LOGPATH/pdu_left_beast-${LOGDATE}.out
    # Note: don't know if there is a difference between left and right yet so testing out both right now
    echo "PDU_RIGHT BEAST" > $LOGPATH/pdu_right_beast-${LOGDATE}.out
    #while sleep 0.5; do
	./pdu/getPDUInfo.sh 1 $PDU_LEFT $BEAST 0 0 >> $LOGPATH/pdu_left_beast-${LOGDATE}.out
	./pdu/getPDUInfo.sh 1 $PDU_RIGHT $BEAST 0 0 >> $LOGPATH/pdu_right_beast-${LOGDATE}.out
    #done
}

powerLog "$1" "$2"
