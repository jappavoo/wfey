LOGS="logs"
declare -i SLEEP_TO_FINISH=3
declare -i SLEEP_TO_RESET=5
declare -i runnercpu=80

declare -a wfeyconfig=(busypoll_nodb_wfey busypoll_db_wfey wfe_nodb_wfey wfe_db_nomon_wfey wfe_db_mon_wfey)
declare -a eventprocCPUs=( "1" )
declare -a sourceCPUs=( "1" "10" "50" )
declare -a eventrate=( "10" "100" "50000" "100000" "150000" )
