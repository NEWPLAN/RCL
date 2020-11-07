.PHONY: clean

CFLAGS  := -Wall -std=c++11 -Os
LD      := g++
LDLIBS  := ${LDLIBS} -lrdmacm -libverbs -lpthread -lglog

APPS    :=  RDMABenchmark

HEADE   := RDMABase.h RDMAServer.h RDMAClient.h

all: ${APPS}

RDMABenchmark: RDMABenchmark.o  RDMABase.o RDMAServer.o RDMAClient.o
	${LD} -o $@ $^ ${LDLIBS}

rdma: rdma.o
	${LD} -o $@ $^ ${LDLIBS}

%.o:%.cpp $(HEADE)
	$(LD) -c $(CFLAGS) $< -o $@

clean:
	rm -f *.o ${APPS} *.txt

server:
	./RDMABenchmark --server 0.0.0.0
client:
	./RDMABenchmark --server 12.12.12.111 --client 12.12.12.112

sync:
	scp RDMABenchmark dictxiong@12.12.12.101:~/RDMABenchmark
	scp RDMABenchmark dictxiong@12.12.12.102:~/RDMABenchmark
	scp RDMABenchmark dictxiong@12.12.12.103:~/RDMABenchmark
	scp RDMABenchmark dictxiong@12.12.12.104:~/RDMABenchmark
	scp RDMABenchmark dictxiong@12.12.12.105:~/RDMABenchmark
	scp RDMABenchmark dictxiong@12.12.12.106:~/RDMABenchmark
	scp RDMABenchmark dictxiong@12.12.12.107:~/RDMABenchmark
	scp RDMABenchmark dictxiong@12.12.12.108:~/RDMABenchmark
	scp RDMABenchmark dictxiong@12.12.12.109:~/RDMABenchmark
	scp RDMABenchmark dictxiong@12.12.12.110:~/RDMABenchmark
	scp RDMABenchmark dictxiong@12.12.12.111:~/RDMABenchmark
	scp RDMABenchmark dictxiong@12.12.12.112:~/RDMABenchmark
	scp RDMABenchmark dictxiong@12.12.12.113:~/RDMABenchmark
	scp RDMABenchmark dictxiong@12.12.12.114:~/RDMABenchmark
	scp RDMABenchmark dictxiong@12.12.12.115:~/RDMABenchmark
	scp RDMABenchmark dictxiong@12.12.12.116:~/RDMABenchmark


timer: timer.cpp clean_timer 
	g++ $< -o $@ -std=c++11 -Os -lpthread -lglog
clean_timer:
	rm -rf timer

