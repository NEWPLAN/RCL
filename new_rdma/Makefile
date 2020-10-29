LOCAL_IP = '$(shell ifconfig |grep "12.12.12.1"| grep -Eo 'inet (addr:)?([0-9]*\.){3}[0-9]*' | grep -Eo '([0-9]*\.){3}[0-9]*' | grep -v '127.0.0.1' | awk '{print $1}')'

all:
	@echo "What you wnat to do?";

sync:
	rm -rf build ;
	echo "your IP is: ",  $(LOCAL_IP);
	@if [ $(LOCAL_IP) = '12.12.12.111' ]; \
	then \
		echo "You are in the local machine 12.12.12.111"; \
	else \
		echo "IP is euqal t: ", $(LOCAL_IP); \
		make copy ;\
	fi

copy:
	@if [ $(LOCAL_IP) != '12.12.12.111' ]; \
	then \
		sleep 2; \
		echo "We are copying data from 12.12.12.111"; \
		scp -r newplan@12.12.12.111:/home/newplan/test/RCL/new_rdma/* /home/newplan/test/RCL/new_rdma; \
	else \
		rm -rf build; \
	fi