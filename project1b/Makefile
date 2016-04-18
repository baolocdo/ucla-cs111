main: client.c server.c
	gcc -o client -g client.c -lpthread -lmcrypt -Wall
	gcc -o server -g server.c -lpthread -lmcrypt -Wall
client: client.c
	gcc -o client -g client.c -lpthread -lmcrypt -Wall
server: server.c
	gcc -o server -g server.c -lpthread -lmcrypt -Wall
clean:
	rm -rf client
	rm -rf server
	rm -rf *.o
	rm -rf *.out
dist:
	rm -rf lab1b-404380075.tar.gz
	tar -zcf lab1b-404380075.tar.gz *.c Makefile README my.key