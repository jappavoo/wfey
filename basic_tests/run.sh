#!/bin/bash

prog=${1:-wfe} # program to run -- wfe, sleep, busy
wsev=${2:-nosev} # whether or not sev should be looping in background
sleep=${3:-1} # how long to sleep for while program runs
repeat=${4:-10} # how many times the experiement should repeat -- every run will output

[ -e ${prog}_${wsev}.txt ] && rm -- ${prog}_${wsev}.txt

for i in $(seq 1 ${repeat})
do
    sensors -u altra_hwmon-isa-0000 | grep -E 'gy14' >> ${prog}_${wsev}.txt
    if [[ "$wsev" != "nosev" ]]; then taskset -c 56 ./sev & spid=$!; fi
    if [[ "$prog" != "sleep" ]]; then taskset -c 13 ./${prog} & cpid=$!; fi
    sleep ${sleep}
    if [[ "$prog" != "sleep" ]]; then kill $cpid; fi
    if [[ "$wsev" != "nosev" ]]; then kill $spid; fi
    sensors -u altra_hwmon-isa-0000 | grep -E 'gy14' >> ${prog}_${wsev}.txt
done

# Output will be J per sec -- W
echo $prog $wsev: wait time = ${sleep}
./sub.sh $sleep < ${prog}_${wsev}.txt
