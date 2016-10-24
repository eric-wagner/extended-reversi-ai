#!/bin/bash

#
# All the files on which the tests should be run.
#
path[0]="RiseOfBattle.map"
path[1]="RiseOfBattle_IG.map"
path[2]="g8_3.map"
path[3]="g8_3_IG.map"
path[4]="g5_3.map"
path[5]="g5_3_IG.map"
path[6]="g5_1.map"
path[7]="g5_1_IG.map"
path[8]="g3_2.map"
path[9]="g3_2_IG.map"
path[10]="FunnelOfDeath.map"
path[11]="FunnelOfDeath_IG.map"
path[12]="g3_3.map"
path[13]="g3_3_IG.map"
path[12]="g3_1.map"
path[13]="g3_1_IG.map"
path[14]="TripleOfTraps.map"
path[15]="TripleOfTraps_IG.map"

# Get length of path array.
numberOfPaths=${#path[@]}

echo "Start"

# use for loop read all nameservers
for (( i=11; i < 12; i++ ));
do
	echo "START_MAP_${path[i]}" >> BENCHMARK.txt
	echo "START_MAP_${path[i]}"
	for (( j=2; j < 6; j++ ));
	do
		echo "DEPTH_$j" >> BENCHMARK.txt
		echo "DEPTH_$j"
		./compiled/Benchmark.exe -m Benchmarking/${path[i]} -d $j -b 5 >> BENCHMARK.txt
	done
done

echo "Finish"