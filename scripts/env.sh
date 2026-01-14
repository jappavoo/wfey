SCRIPTDIR=$(dirname -- "$BASH_SOURCE")

## MetaVars -- Per Machine/Test ##
#source ${SCRIPTDIR}/no_module_env.sh
source ${SCRIPTDIR}/module_env.sh

## Uncomment if you want the energy numbers to be gathered
#  in the benchmark per epThread
#  If commented out -- energy gathering will depend on what
#  energy script is being used
BM_ENERGY=1

## Name of experiment folder -- (all stored in parent folder 'logs') ##
LOGS="test"
PRETESTPATH="logs/$LOGS/pretest/"

## How many times to run experiment ##
times_to_run=3

## What CPU should all scripts run on ##
# Note: this should be the same in the benchmark as well (not automated)
declare -i runnercpu=$(($(${SCRIPTDIR}/find_cores_per_socket.sh) - 1))

## What Configs to run in this experiment ##
declare -a wfeyconfig=(busypoll_nodb_wfey busypoll_db_wfey wfe_nodb_wfey wfe_db_nomon_wfey wfe_db_mon_wfey)

## Benchmark Arg Configs ##
declare -a eventprocCPUs=( "1" )

# Full Sweep
declare -a sourceCPUs=( "1" "10" "50" )
declare -a eventrate=( "0" "10" "100" "1000" "100000")

# None vs Many Events
#declare -a sourceCPUs=( "1" ) 
#declare -a eventrate=( "0" "50000")
