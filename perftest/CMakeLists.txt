CMAKE_MINIMUM_REQUIRED(VERSION 3.0) #cmake版本要求
project(Benchmark)

message("Building make for Benchmark") #打印消息
message("add -DDYNAMIC_E=1 when build dynamic class E, the default is static")
set(CMAKE_BUILD_TYPE "Debug")

set(SUB_MODULE_PATH "./core")
SET(CMAKE_C_COMPILER "gcc")
SET(CMAKE_CXX_COMPILER "g++")

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11) # 设置std=c++11

# 设置debug或者release的编译参数
set(CMAKE_C_FLAGS_DEBUG "$ENV{CFLAGS} -D_GNU_SOURCE -DHAVE_CONFIG_H -O0 -Wall -g -ggdb -fPIC")
set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -D_GNU_SOURCE -DHAVE_CONFIG_H -O0 -Wall -g -ggdb -fPIC")
#set(CMAKE_C_FLAGS_DEBUG "$ENV{CFLAGS} -D_GNU_SOURCE -DHAVE_CONFIG_H -Os -fPIC")
#set(CMAKE_C_FLAGS_RELEASE "$ENV{CFLAGS} -D_GNU_SOURCE -DHAVE_CONFIG_H -Os -Wall -fPIC")
#set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g -ggdb -fPIC")
# set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -Os -Wall -fPIC")


# find_package(glog 0.4.0 REQUIRED)
# include_directories(BEFORE ${GLOG_INCLUDE_DIRS}) 
# set(SYSTEM_LIBRARIES ${SYSTEM_LIBRARIES} ${GLOG_LIBRARY}) 

find_path(IBVERBS_INCLUDE_DIR
	NAMES infiniband/verbs.h
)

find_library(IBVERBS_LIBRARY
	NAMES ibverbs
)

find_library(GLOG_LIBRARY
	NAMES glog
)
message(${GLOG_LIBRARY})
message(${IBVERBS_LIBRARY})

set(SUB_MODULE_PATH "./src")

aux_source_directory(${SUB_MODULE_PATH} SRC_LIST)

#分别指定编译include和链接link的搜索目录
include_directories(${SUB_MODULE_PATH})

link_directories(${SUB_MODULE_PATH})

#添加一个子模块，编译到lib目录下去
#add_subdirectory(${SUB_MODULE_PATH}) # build uitls subsystem
#add_subdirectory(./computation) # build computation subsystem
#add_subdirectory(./network) # build network subsystem
#add_subdirectory(./datasystem) # build data subsystem
#add_subdirectory(./topology) # build data subsystem

#设置编译目标
add_executable(ribwrite ${SRC_LIST}
                                    test-example/write_bw.cpp)
#添加编译目标依赖
target_link_libraries(ribwrite 
                    glog
                    pthread
                    -lrdmacm
                    ibverbs
                    -libumad
                    -lm )