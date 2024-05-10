#!/bin/bash -e

killall sigw sigq || true

./sigw &
sleep 1 

for I in $(seq 1 1000); do 
	echo -n "Hello ${I} " | ./sigq sigw
done

sleep 1 
./sigq QUIT;
