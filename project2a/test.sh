#!/bin/bash

rm -rf stderr.txt
rm -rf stdout.txt

threads=(1 2 4 8 12 16 20 24 28 32)
iterations=(1 10 100 200 500 1000 2000 5000 10000 100000)

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
