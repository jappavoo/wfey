ENERGYSCRIPT=$(dirname -- "$BASH_SOURCE")/beast_hwmon.sh

# Note: the hwmon values need any to reset/reach peak
# They are a rate of power being used
declare -i SLEEP_TO_FINISH=3
declare -i SLEEP_TO_RESET=5
