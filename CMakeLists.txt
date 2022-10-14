cmake_minimum_required(VERSION 3.16)

project(linenoise-niz
  VERSION 1.0.0
  DESCRIPTION "Modified fork of linenoise"
  LANGUAGES C
)

OPTION(WITH_DEBUG_SYMBOLS "Include symbols for debugging" OFF)

configure_file(config.h.in ${PROJECT_BINARY_DIR}/config.h)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wmissing-prototypes -Werror -Wno-unused-result")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
if(WITH_DEBUG_SYMBOLS)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")
endif(WITH_DEBUG_SYMBOLS)

add_library(linenoise SHARED
  linenoise.c 
  include/linenoise.h 
  buffer.c
  buffer.h
  linenoise_private.h
  linenoise_key_binding.c
)

target_include_directories(linenoise 
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE ${PROJECT_BINARY_DIR}
)

file(GLOB headers include/*.h)
install(FILES ${headers} DESTINATION include/linenoise)
install(TARGETS linenoise ARCHIVE DESTINATION lib)
