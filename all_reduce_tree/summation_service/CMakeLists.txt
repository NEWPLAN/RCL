CMAKE_MINIMUM_REQUIRED(VERSION 3.0) #cmake版本要求
project(summation_benchmark)

message("Building RDMA Benchmark") #打印消息
message("add -DDYNAMIC_E=1 when build dynamic class E, the default is static")


SET(CMAKE_C_COMPILER "gcc")
SET(CMAKE_CXX_COMPILER "g++")


set(CMAKE_CXX_STANDARD 17)
#set(CMAKE_CXX_STANDARD_REQUIRED ON)

#set(CMAKE_BUILD_TYPE debug)
set(CMAKE_BUILD_TYPE release)



set(SUB_MODULE_PATH "./lib")
#set(CMAKE_CXX_STANDARD 20) # 设置std=c++2a


# 设置debug或者release的编译参数
set(CMAKE_CXX_FLAGS_DEBUG "${CXXFLAGS} ${CMAKE_CXX_FLAGS_DEBUG} -O0 -Wall -Wextra -g -ggdb -fPIC -fopenmp")
set(CMAKE_CXX_FLAGS_RELEASE "${CXXFLAGS} ${CMAKE_CXX_FLAGS_RELEASE} -O2 -Wall -Wextra -fPIC -fopenmp -mavx2 -fopt-info")

# collect all message to "SRC_LIST" <-- ./*.cpp
aux_source_directory(./example SRC_LIST)

#分别指定编译include和链接link的搜索目录
include_directories(./include)
link_directories(${SUB_MODULE_PATH})

#添加一个子模块，编译到lib目录下去
add_subdirectory(${SUB_MODULE_PATH}) # add a submodule, and compile it to the "build/lib"
#设置编译目标
add_executable(summation_benchmark ${SRC_LIST})
#添加编译目标依赖
target_link_libraries(summation_benchmark 
                        glog 
                        pthread 
                        summationwrapper)
