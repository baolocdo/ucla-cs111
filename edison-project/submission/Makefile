LIBS = -L/usr/lib -lssl -lcrypto -lpthread
OBJS = tls_header.o

all: tls_client start_attack set_rate

tls_client: tls_client.c tls_header.o
	gcc -w -o tls_client tls_client.c $(OBJS) $(LIBS)

set_rate: set_rate.c tls_header.o
	gcc -o set_rate set_rate.c $(OBJS) $(LIBS)
	
tls_header.o: tls_header.c
	gcc -c -o tls_header.o tls_header.c $(LIBS)

start_attack: start_attack.c
	gcc -o start_attack start_attack.c $(OBJS) $(LIBS)

clean:
	rm -f set_rate start_rate tls_client tls_header.o

dist:
	rm -rf lab4-404380075.tar.gz
	tar -zcf lab4-404380075.tar.gz tls_client.c Makefile README *.png Lab4-E-2.txt

clobber: 
	make clean
	rm -f *.txt *~
