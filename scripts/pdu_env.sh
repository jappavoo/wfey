ENERGYSCRIPT=$(dirname -- "$BASH_SOURCE")/pdu.sh
ENERGYTYPE="WMOD"

# Note: don't know if any of PDU values need time to reset
# Will assume they don't until proven otherwise
declare -i SLEEP_TO_FINISH=5 #empirically pdu updates every 8-10sec
declare -i SLEEP_TO_RESET=0
