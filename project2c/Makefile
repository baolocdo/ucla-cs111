main: lab2c.c SortedList.c
	gcc -o lab2c -g lab2c.c SortedList.c -pthread -Wall
clean:
	rm -rf lab2c
	rm -rf *.o
	rm -rf *.out
dist:
	rm -rf lab2c-404380075.tar.gz
	tar -zcf lab2c-404380075.tar.gz *.c *.h Makefile README *.png