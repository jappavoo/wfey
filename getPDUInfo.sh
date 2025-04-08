#!/bin/bash

###export SSH_ASKPASS=./pw.sh
###export SSH_ASKPASS_REQUIRE=force

PASSWORD=$(cat ./pw.txt)

leftPDU="10.89.19.16"
rightPDU="10.89.19.17"

leftDEV="DE55D90D851900C3"
rightDEV="1F76D90D851900C3"

beastOUTLET="25"
donOUTLET="30"

apparentPower="9"
current="4"
realPower="8"
currentCrestFactor="14"
voltage="0"
energy="11"
powerFactor="10"

info () {
    echo "USAGE: $0 or $0 <loop #> <pdu> <outlet> (<measurement> <num?>)"
    echo "Password -- you should have your password in plain text"
    echo -e "\t in a file called pw.txt."
    echo "PDU -- Which PDU you are reading from, left or right"
    echo -e "\t This corresponds to the deviceID you are reading from"
    echo "DEV -- Set automatically based on the PDU you are reading from"
    echo "OUT -- The outlet you want to read from"
    echo -e "\t Current options: abeast1-sesa (25), don-sesa (30)"
    echo "MES -- If you want to get specfic measurement"
    echo -e "\t Current options: [Skip = 0, apparentPower = 1, current = 2, realPower = 3, currentCrestFactor = 4, voltage = 5, energy = 6, powerFactor= 7]"
    echo "NUM -- If you want only the number, let it be so. 1=on 0=off"
    echo "LOOP -- The number of times this value will be gathered"
    echo "DELAY -- Delay between loops"
}

setPDU() {
    while true; do
	read -n1 -p "Which PDU would you like to read from? [Left: 0, Right: 1] " ANS
	case $ANS in 
	    '0') 
		PDU=$leftPDU
		DEV=$leftDEV
		break;;
	    '1') 
		PDU=$rightPDU
		DEV=$rightDEV
		break;;
	    'h')
		info;;
	    'q')
		exit 0;;
	    *) 
		echo "Not a valid input. Try again";;
	esac
    done
    echo -e "\nPDU value: $PDU"
}

setOutlet() {
    while true; do
	read -n1 -p "Which outlet would you like to read from? [Don: 0, Beast: 1] " ANS
	case $ANS in 
	    '0') 
		OUT=$donOUTLET
		break;;
	    '1') 
		OUT=$beastOUTLET
		break;;
	    'h')
		info;;
	    'q')
		exit 0;;
	    *) 
		echo "Not a valid input. Try again";;
	esac
    done
    echo -e "\nOutlet value: $OUT"
}

decodeMeasurement() {
    index=$1
    case $index in 
	'0') 
	    MES=""
	    break;;
	'1') 
	    MES=$apparentPower
	    break;;
	'2') 
	    MES=$current
	    break;;
	'3') 
	    MES=$realPower
	    break;;
	'4') 
	    MES=$currentCrestFactor
	    break;;
	'5') 
	    MES=$voltage
	    break;;
	'6') 
	    MES=$energy
	    break;;
	'7') 
	    MES=$powerFactor
	    break;;
    esac
}

setMeasurement() {
    while true; do
	read -n1 -p "If you would like to read a specific measurement, you can specify here, or you can skip this step [Skip = 0, apparentPower= 1, current = 2, realPower = 3, currentCrestFactor = 4, voltage = 5, energy = 6, powerFactor= 7]" ANS
	case $ANS in 
	    'h')
		info
		break;;
	    'q')
		exit 0;;
	    *)
		if (($ANS<=7 && $ANS>=0)); then
		    decodeMeasurement $ANS
		    break
		else
		    echo "Not a valid input. Try again"
		fi
		;;
	esac
    done
    echo -e "\nMeasurement value: $MES"
}

setNum() {
    while true; do
	read -n1 -p "Would you like only the measurment value? (Disregard if measurement specification was skipped) [Show all: 0, Show Value: 1] " ANS
	case $ANS in 
	    '0') 
		NUM=0
		break;;
	    '1') 
		NUM=1
		break;;
	    'h')
		info;;
	    'q')
		exit 0;;
	    *) 
		echo "Not a valid input. Try again";;
	esac
    done
    echo -e "\nNum value: $NUM"
}

setLoop() {
    read -p "How many times would you like to gather this data" LOOP
    echo -e "\nLoop value: $LOOP"
}

setVars () {
    echo "Configuring script... If at any time you want to quit, press 'q'."
    echo "If you would like more information, press 'h'"
    setLoop
    setPDU
    setOutlet
    setMeasurement
    setNum
}

argParse() {
    #echo "$1 $2 $3 $4 $5"
    LOOP=$1
    PDULoc=$2
    if [[ "$PDULoc" -eq "left" ]]; then
	PDU=$leftPDU
	DEV=$leftDEV
    elif [[ "$PDULoc" = "right" ]]; then
	PDU=$rightPDU
	DEV=$rightDEV
    else
	echo "Invalid PDU"
	info
	exit -1
    fi
    OUT=$3

    if [[ "$OUT" -ne $donOUTLET && "$OUT" -ne $beastOUTLET ]]; then
	echo "Invalid outlet"
	info
	exit -1
    fi
    
    decodeMeasurement $4
    NUM=$5
}

echo $#
if [ "$#" -eq 0 ]; then
    echo "Tutorial mode"
    setVars
elif [ "$#" -le 2 ]; then  # Need at least LOOP, PDU, and OUT
    echo "Not enough parameters"
    info
    exit -1
else
    echo "Expert Mode"
    argParse $1 $2 $3 $4 $5
fi

echo "./expect_test.exp $PASSWORD $LOOP $PDU $DEV $OUT $MES $NUM"

./expect_test.exp $PASSWORD $LOOP $PDU $DEV $OUT $MES $NUM

