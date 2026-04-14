#!/bin/bash

#set -x 

progs=(wfe busy sleep)
sev=(wsev nosev)

sleep=5
repeat=10

gnuplotcmd="set ylabel 'J'; set boxwidth 0.5; set style fill solid; set yrange[0.01:]; set key off; set logscale y 10; plot 'tmp.txt' using 1:3:xtic(2) with boxes, '' using 0:(\$3+.5):(sprintf('%3.2f', \$3)) with labels"

for s in ${sev[*]}; do
    [ -e tmp.txt ] && rm -- tmp.txt
    index=0
    for p in ${progs[*]}; do
	output=$(./run.sh $p $s $sleep $repeat)
	echo "$index $p $output" >> tmp.txt
	((index++))
    done
    title="${s} -- sleep:${sleep} repeats:${repeat}"
    #plot_cmd="set term png; set output '${title}.png'; set title '$title'; $gnuplotcmd"
    plot_cmd="set term dumb; set title '$title'; $gnuplotcmd"
    gnuplot -e "$plot_cmd"
done
