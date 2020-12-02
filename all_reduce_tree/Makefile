LOCAL_IP = '$(shell ifconfig |grep "12.12.12.1"| grep -Eo 'inet (addr:)?([0-9]*\.){3}[0-9]*' | grep -Eo '([0-9]*\.){3}[0-9]*' | grep -v '127.0.0.1' | awk '{print $1}')'

TARGET_FOLD:=all_reduce_tree

all:
	@echo "What you wnat to do?";
	@echo "make sync: 同步代码";
	@echo "make copy: 复制代码";

sync:
	rm -rf build ;
	echo "your IP is: ",  $(LOCAL_IP);
	@if [ $(LOCAL_IP) = '12.12.12.111' ]; \
	then \
		echo "You are in the master node: 12.12.12.111"; \
	else \
		echo "Your are in a slaver node: "  $(LOCAL_IP); \
		make copy ;\
	fi

copy:
	@if [ $(LOCAL_IP) != '12.12.12.111' ]; \
	then \
		sleep 2; \
		echo "We are copying data from 12.12.12.111"; \
		sshpass -p ' ' scp -r newplan@12.12.12.111:/home/newplan/test/RCL/$(TARGET_FOLD)/* /home/newplan/test/RCL/$(TARGET_FOLD); \
	else \
		rm -rf build; \
	fi

build:clean
	mkdir -p build; cd build; cmake ..; make -j32;
	cd build;


.PHONY:clean
clean:
	rm -rf ./build;