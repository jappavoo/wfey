#!/bin/bash

div=$1

sum=0
count=0
while read d val1 && read d val2
do
    count=$((count + 1))
    sub_val=$(echo "scale=3; ($val2 - $val1) / $div" | bc -l)
    sum=$(echo "scale=3; $sum + $sub_val" | bc -l)
done

avg=$(echo "scale=3; $sum / $count" | bc -l)
echo $avg J
