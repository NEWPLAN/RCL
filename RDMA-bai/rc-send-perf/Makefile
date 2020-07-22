.PHONY: all clean

CC = gcc
LDLIBS = -lrdmacm -libverbs -lpthread
CFLAGS = -Wall -g 
APPS = rc-send-perf-client rc-send-perf-server
OBJS = client.o server.o common.o

all: ${APPS}

rc-send-perf-server: server.o common.o
	$(CC) $(CFLAGS) -o rc-send-perf-server server.o common.o $(LDLIBS) 	

rc-send-perf-client: client.o common.o
	$(CC) $(CFLAGS) -o rc-send-perf-client client.o common.o $(LDLIBS) 

server.o: server.c common.h  
	$(CC) $(CFLAGS) -c -o server.o server.c

client.o: client.c common.h  
	$(CC) $(CFLAGS) -c -o client.o client.c

common.o: common.c common.h  
	$(CC) $(CFLAGS) -c -o common.o common.c

clean:
	rm -f *.o $(APPS)