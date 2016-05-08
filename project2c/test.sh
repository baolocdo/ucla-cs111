#!/bin/bash

rm -rf stderr.txt
rm -rf stdout.txt

rm -rf stdout_yield.txt
rm -rf stdout_yield.txt

rm -rf stdout_sync.txt
rm -rf stdout_sync.txt

threads=8
iterations=10000
lists=(64 32 16 8 4 2 1)
sync=(m s n)

for il in "${lists[@]}"; do
	for isync in "${sync[@]}"; do
		echo "./lab2c --iterations=$iterations --threads=$threads --sync=$isync --lists=$il"
		echo "./lab2c --iterations=$iterations --threads=$threads --sync=$isync --lists=$il" >> stdout_sync.txt
		echo "./lab2c --iterations=$iterations --threads=$threads --sync=$isync --lists=$il" >> stderr_sync.txt
		for i in `seq 1 10`; do
			./lab2c --iterations=$iterations --threads=$threads --sync=$isync --lists=$il 2>>stderr_sync.txt 1>>stdout_sync.txt
		done
	done
done