cmake_minimum_required(VERSION 2.6)
project(linenoise C)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wmissing-prototypes -Werror")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")

add_library(linenoise linenoise.c linenoise.h)
target_include_directories(linenoise PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})


file(GLOB headers *.h)
install(FILES ${headers} DESTINATION include/linenoise)
install(TARGETS linenoise ARCHIVE DESTINATION lib)
