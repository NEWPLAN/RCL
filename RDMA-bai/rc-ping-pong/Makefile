.PHONY: all clean

CC = gcc
LDLIBS = -lrdmacm -libverbs -lpthread
CFLAGS = -Wall -g 
APPS = rc-pingpong-client rc-pingpong-server
OBJS = client.o server.o common.o

all: ${APPS}

rc-pingpong-server: server.o common.o
	$(CC) $(CFLAGS) -o rc-pingpong-server server.o common.o $(LDLIBS) 	

rc-pingpong-client: client.o common.o
	$(CC) $(CFLAGS) -o rc-pingpong-client client.o common.o $(LDLIBS) 

server.o: server.c common.h  
	$(CC) $(CFLAGS) -c -o server.o server.c

client.o: client.c common.h  
	$(CC) $(CFLAGS) -c -o client.o client.c

common.o: common.c common.h  
	$(CC) $(CFLAGS) -c -o common.o common.c

clean:
	rm -f *.o $(APPS)