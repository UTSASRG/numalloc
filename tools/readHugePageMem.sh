#!/bin/bash
max=0
while :
do
  sum=0
  for i in {0..7}; do
    total=`cat /sys/devices/system/node/node$i/meminfo | fgrep HugePages_Total |grep -Eo '[0-9]+$'`
    free=`cat /sys/devices/system/node/node$i/meminfo | fgrep HugePages_Free |grep -Eo '[0-9]+$'`
    let sum=sum+total-free
  done
  if [ $sum -gt $max ]
  then
    max=$sum
  fi
  echo $max
  sleep 0.5
done
