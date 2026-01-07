#!/bin/bash

prog=${1:-wfe}
wsev=${2:-nosev}
sleep=${3:-1}
repeat=${4:-10}

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

echo $prog $wsev: wait time = ${sleep}
./sub.sh $sleep < ${prog}_${wsev}.txt
