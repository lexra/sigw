#!/bin/bash

killall -9 sigw || true

./sigw &
sleep 1

for I in $(seq 1 1024); do 
	echo -n "Hello ${I} " | ./sigq sigw
done
echo -n VERIFY | ./sigq sigw

sleep 1 
echo -n QUIT | ./sigq sigw
