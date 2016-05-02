main: lab2b.c SortedList.c
	gcc -o lab2b -g lab2b.c SortedList.c -pthread -Wall
clean:
	rm -rf lab2b
	rm -rf *.o
	rm -rf *.out
dist:
	rm -rf lab2b-404380075.tar.gz
	tar -zcf lab2b-404380075.tar.gz *.c Makefile README *.png test.sh