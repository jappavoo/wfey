SCRIPTDIR=$(dirname -- "$BASH_SOURCE")

## MetaVars -- Per Machine/Test ##
#source ${SCRIPTDIR}/no_module_env.sh
#source ${SCRIPTDIR}/module_env.sh
source ${SCRIPTDIR}/pdu_env.sh

##  If uncommented -- energy gathering will happen in the background
#  depending on what ENERGYSCRIPT is declared in above env file
## If commented --  energy numbers are only gathered in the
#  benchmark (per epThread)
## For more specifics looks into the readme
BG_TASK=1

## Name of experiment folder -- (all stored in parent folder 'logs') ##
LOGS="more_sources"
#LOGS="full_parse"
#LOGS="test"
PRETESTPATH="logs/$LOGS/pretest/"

## How many times to run experiment ##
times_to_run=5

## What CPU should all scripts run on ##
# Note: this should be the same in the benchmark as well (not automated)
declare -i runnercpu=0

## What Configs to run in this experiment ##
declare -a wfeyconfig=(busypoll_nodb busypoll_db wfe_nodb wfe_db_nomon wfe_db_mon)

## Benchmark Arg Configs ##
declare -a eventprocCPUs=( "1" )

# Full Sweep
# declare -a eventrate=( "0" "10" "100" "1000" "10000")
# declare -a sourceCPUs=( "1" "10" "50" )
# declare -a memperc=( "0" "25" "50" "75" "100" )
declare -a eventrate=( "0" "10" "100" "1000" "10000")
declare -a sourceCPUs=( "1" "10" )
declare -a memperc=( "0" "50" "100" )

# None vs Many Events
# declare -a eventrate=( "0" "50000")
# declare -a sourceCPUs=( "1" "10" ) 
# declare -a memperc=( "0" "100" )

