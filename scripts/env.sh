SCRIPTDIR=$(dirname -- "$BASH_SOURCE")
LOGS="logs"

declare -i runnercpu=$(($(${SCRIPTDIR}/find_cores_per_socket.sh) - 1))

declare -a wfeyconfig=(busypoll_nodb_wfey busypoll_db_wfey wfe_nodb_wfey wfe_db_nomon_wfey wfe_db_mon_wfey)
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
