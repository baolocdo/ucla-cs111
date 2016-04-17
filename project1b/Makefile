main: client.c server.c
	gcc -o client -g client.c -lpthread -Wall
	gcc -o server -g server.c -lpthread -Wall
client: client.c
	gcc -o client -g client.c -lpthread -Wall
server: server.c
	gcc -o server -g server.c -lpthread -Wall
clean:
	rm -rf client
	rm -rf server
	rm -rf *.o
	rm -rf *.out
dist:
	rm -rf lab1b-404380075.tar.gz
	tar -zcf lab1b-404380075.tar.gz *.c Makefile README