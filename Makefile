CC = gcc
CFLAGS = -Wall -I.
DEPS = util.h
OBJ = server.o client.o 

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

all: server client

server: server.o
	$(CC) -o $@ $^ $(CFLAGS)

client: client.o
	$(CC) -o $@ $^ $(CFLAGS)

srv_1: server
	./server dictionary.txt 1

srv_2: server
	./server dictionary.txt 2

srv_4: server
	./server dictionary.txt 4

cli: client
	./client

clean:
	$(RM) count *.o server client output*