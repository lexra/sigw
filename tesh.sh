#!/bin/bash -e

killall sigw sigq || true

./sigw &
sleep 2

for I in $(seq 1 1000); do 
	./sigq $I
done

./sigq QUIT;
