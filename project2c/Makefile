main: lab2c.c SortedList.c
	gcc -o lab2c -Wall -pg lab2c.c SortedList.c -pthread
clean:
	rm -rf lab2c
	rm -rf *.o
	rm -rf *.out
dist:
	rm -rf lab2c-404380075.tar.gz
	tar -zcf lab2c-404380075.tar.gz *.c *.h Makefile README *.png