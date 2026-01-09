SCRIPTDIR=$(dirname -- "$BASH_SOURCE")

## Name of experiment folder -- (all stored in parent folder 'logs') ##
LOGS="logs"

## How many times to run experiment ##
times_to_run=1 

## What CPU should all scripts run on ##
# Note: this should be the same in the benchmark as well (not automated)
declare -i runnercpu=$(($(${SCRIPTDIR}/find_cores_per_socket.sh) - 1))

## What Configs to run in this experiment ##
declare -a wfeyconfig=(busypoll_nodb_wfey busypoll_db_wfey wfe_nodb_wfey wfe_db_nomon_wfey wfe_db_mon_wfey)

## Benchmark Arg Configs ##
declare -a eventprocCPUs=( "1" )

# Full Sweep
#declare -a sourceCPUs=( "1" "10" "50" )
#declare -a eventrate=( "10" "100" "50000" "100000" "150000" )

# None vs Many Events
declare -a sourceCPUs=( "1" ) 
declare -a eventrate=( "0" "50000")


## MetaVars -- Per Machine/Test ##

#source ${SCRIPTDIR}/no_module_env.sh
source ${SCRIPTDIR}/module_env.sh
