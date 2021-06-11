#! /bin/bash

for x in 0 1 2 3 4 5 6 7 8 9 
do
	echo "remove $x" > /proc/modlist
	sleep 1
done 