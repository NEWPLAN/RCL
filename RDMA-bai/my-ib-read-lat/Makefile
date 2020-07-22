.PHONY: all clean

CC = gcc
LDLIBS = -libverbs
CFLAGS = -Wall -g 
APPS = ib_read_lat_client ib_read_lat_server
OBJS = client.o server.o common.o

all: ${APPS}

ib_read_lat_server: server.o common.o 
	$(CC) $(CFLAGS) -o ib_read_lat_server server.o common.o $(LDLIBS)

ib_read_lat_client: client.o common.o 
	$(CC) $(CFLAGS) -o ib_read_lat_client client.o common.o $(LDLIBS)

server.o: server.c common.h 
	$(CC) $(CFLAGS) -c -o server.o server.c

client.o: client.c common.h 
	$(CC) $(CFLAGS) -c -o client.o client.c

common.o: common.c common.h
	$(CC) $(CFLAGS) -c -o common.o common.c

clean:
	rm -f *.o $(APPS)