declare -i runnercpu = 80

declare -a wfeyconfig=(busypoll_nodb_wfey busypoll_db_wfey wfe_nodb_wfey wfe_db_nomon_wfey wfe_db_mon_wfey)
declare -a eventprocCPUs=( "1" )
#declare -a sourceCPUs=( "2" "2 3" "2 81" "81 82" "2 3 4" "2 3 4 5 6" )
#declare -a sourceCPUs=( "2" "2 3" "2 3 4" "2 3 4 5 6" )
declare -a sourceCPUs=( "1" "5" "10" )
declare -a sleeptime=( "0.001" "0.01" "0.1" )
declare -a numevents=( "10" "100" "1000" )
