.PHONY: clean

CXXFLAGS  := -Wall -Os -std=c++11
LD      := g++
LDLIBS  := ${LDLIBS} -Os -lrdmacm -libverbs -lpthread -lglog

APPS    := client server

all: ${APPS}

client: common.o client.o
	${LD} -o $@ $^ ${LDLIBS}

server: common.o server.o
	${LD} -o $@ $^ ${LDLIBS}

clean:
	rm -f *.o ${APPS}

