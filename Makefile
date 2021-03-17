#Makefile for TCP echo client-server service for Team 6 in ECEN602

output: server.o client.o
	gcc server.o -o server
	gcc client.o -o client

server.o: server.c
	gcc -c server.c

client.o: client.c
	gcc -c client.c

clean:
	rm -f client server *.o core