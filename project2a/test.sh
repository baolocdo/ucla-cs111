#!/bin/bash

echo "./lab2a --iterations=10 --threads=1"
for i in `seq 1 10`; do
	./lab2a --iterations=10 --threads=1 2>stderr.txt 1>stdout.txt
done