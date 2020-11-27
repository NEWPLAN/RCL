CMAKE_MINIMUM_REQUIRED(VERSION 3.0) #cmake版本要求
project(AggStandalone)

message("Building make for Aggtest-Standalone") #打印消息
set(CMAKE_BUILD_TYPE "Debug")

set(CMAKE_CXX_STANDARD 11) # 设置std=c++11

# 设置debug或者release的编译参数
set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g -ggdb -fPIC")
#set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -Os -Wall -fPIC")

#设置编译目标
add_executable(AggStandalone ./aggtest_standalone.cpp)
