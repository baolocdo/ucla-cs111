#!/bin/bash

rm -rf stderr.txt
rm -rf stdout.txt

rm -rf stdout_yield.txt
rm -rf stdout_yield.txt

rm -rf stdout_sync.txt
rm -rf stdout_sync.txt

threads=(1 2 4 8 12 16 20 24 28 32)
iterations=(1 10 100 200 500 1000 2000 5000 10000 100000)
sync=(m c s n)

# Part one
for it in "${threads[@]}"; do
	for ii in "${iterations[@]}"; do
		echo "./lab2a --iterations=$ii --threads=$it"
		echo "./lab2a --iterations=$ii --threads=$it" >> stdout.txt
		echo "./lab2a --iterations=$ii --threads=$it" >> stderr.txt
		for i in `seq 1 10`; do
			./lab2a --iterations=$ii --threads=$it 2>>stderr.txt 1>>stdout.txt
		done
	done
done

# Part two
for it in "${threads[@]}"; do
	for ii in "${iterations[@]}"; do
		echo "./lab2a --iterations=$ii --threads=$it --yield"
		echo "./lab2a --iterations=$ii --threads=$it --yield" >> stdout_yield.txt
		echo "./lab2a --iterations=$ii --threads=$it --yield" >> stderr_yield.txt
		for i in `seq 1 10`; do
			./lab2a --iterations=$ii --threads=$it --yield 2>>stderr_yield.txt 1>>stdout_yield.txt
		done
	done
done

# Part three
icount=10000
for it in "${threads[@]}"; do
	for isync in "${sync[@]}"; do
		echo "./lab2a --iterations=$icount --threads=$it --sync=$isync"
		echo "./lab2a --iterations=$icount --threads=$it --sync=$isync" >> stdout_sync.txt
		echo "./lab2a --iterations=$icount --threads=$it --sync=$isync" >> stderr_sync.txt
		for i in `seq 1 10`; do
			./lab2a --iterations=$icount --threads=$it --sync=$isync 2>>stderr_yield.txt 1>>stdout_yield.txt
		done
	done
done