#!/bin/bash

rm -rf stderr.txt
rm -rf stdout.txt

rm -rf stdout_yield.txt
rm -rf stdout_yield.txt

rm -rf stdout_sync.txt
rm -rf stdout_sync.txt

threads=(1 2 4 6 8 10 12)
iterations=(1 100 200 400 600 800 1000)
sync=(m s n)
yields=(i d is ds)

# Part one: single thread running time
for ii in "${iterations[@]}"; do
	echo "./lab2b --iterations=$ii --threads=1"
	echo "./lab2b --iterations=$ii --threads=1" >> stdout.txt
	echo "./lab2b --iterations=$ii --threads=1" >> stderr.txt
	for i in `seq 1 10`; do
		./lab2b --iterations=$ii --threads=1 2>>stderr.txt 1>>stdout.txt
	done
done

# Part two: demonstrating failure in case of yields
for it in "${threads[@]}"; do
	for ii in "${iterations[@]}"; do
		for iy in "${yields[@]}"; do
			echo "./lab2b --iterations=$ii --threads=$it --yield=$iy"
			echo "./lab2b --iterations=$ii --threads=$it --yield=$iy" >> stdout_yield.txt
			echo "./lab2b --iterations=$ii --threads=$it --yield=$iy" >> stderr_yield.txt
			for i in `seq 1 10`; do
				./lab2b --iterations=$ii --threads=$it --yield=$iy 2>>stderr_yield.txt 1>>stdout_yield.txt
			done
		done
	done
done

# Part three
icount=1000
for it in "${threads[@]}"; do
	for isync in "${sync[@]}"; do
		echo "./lab2b --iterations=$icount --threads=$it --sync=$isync"
		echo "./lab2b --iterations=$icount --threads=$it --sync=$isync" >> stdout_sync.txt
		echo "./lab2b --iterations=$icount --threads=$it --sync=$isync" >> stderr_sync.txt
		for i in `seq 1 10`; do
			./lab2b --iterations=$icount --threads=$it --sync=$isync 2>>stderr_sync.txt 1>>stdout_sync.txt
		done
	done
done