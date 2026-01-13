ENERGYSCRIPT=$(dirname -- "$BASH_SOURCE")/beast_module.sh
ENERGYTYPE="WMOD"

# Note: the module values do not need any time to reset/reach peak
# They are an instantaneous value
declare -i SLEEP_TO_FINISH=0
declare -i SLEEP_TO_RESET=0
