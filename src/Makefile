all: service benchmark

benchmark_objs := benchmark.o preconnector.o
include_header := preconnector.h util.h
CXXFLAGS += -std=c++11 -g
LINKFLAGS += -lpthread -libverbs -lglog

service: service.cpp 
	g++ $^ -o $@ -g $(LINKFLAGS)

benchmark: $(benchmark_objs)
	g++ $(CXXFLAGS) $^ -o $@  $(LINKFLAGS)


%.o:%.cpp $(include_header)
	echo "here"
	g++ -c $< -o $@ $(CXXFLAGS)
clean:
	rm -rf ./*.o ./service benchmark