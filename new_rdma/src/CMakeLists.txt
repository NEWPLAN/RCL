aux_source_directory(. DIR_LIB_SRCS)
MESSAGE("building rdmacomm ... ")
add_library(rdmacomm SHARED ${DIR_LIB_SRCS} )
target_link_libraries(rdmacomm ibverbs)