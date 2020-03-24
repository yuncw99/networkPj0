all : Client Server

Client : client.c
	gcc -o Client client.c

Server : server.c
	gcc -o Server server.c
