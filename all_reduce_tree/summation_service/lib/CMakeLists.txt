aux_source_directory(. DIR_LIB_SRCS)
MESSAGE("building summationwrapper ... ")

# 设置debug或者release的编译参数
set(CMAKE_CXX_FLAGS_DEBUG  "${CMAKE_CXX_FLAGS_DEBUG} -fopenmp")
set(CMAKE_CXX_FLAGS_RELEASE  "${CMAKE_CXX_FLAGS_RELEASE} -fopenmp")

add_library(summationwrapper SHARED ${DIR_LIB_SRCS} )
target_link_libraries(summationwrapper pthread)

add_library(summationwrapper-static STATIC ${DIR_LIB_SRCS} )
target_link_libraries(summationwrapper-static pthread)