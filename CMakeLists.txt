cmake_minimum_required(VERSION 3.18)
project(linenoise C)

OPTION(WITH_HINTS "Support hints" ON)
OPTION(WITH_KEY_BINDING "Support key binding" OFF)
OPTION(QUERY_TERMINAL_FOR_WIDTH "Attempt to get terminal width using ESCAPE codes if ioctl calls fail" ON)
OPTION(WITH_DEBUG_SYMBOLS "Include symbols for debugging" OFF)

configure_file(config.h.in ${PROJECT_BINARY_DIR}/config.h)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wmissing-prototypes -Werror")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
if(WITH_DEBUG_SYMBOLS)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")
endif(WITH_DEBUG_SYMBOLS)

add_library(linenoise 
  linenoise.c 
  linenoise.h 
  buffer.c
  buffer.h
  linenoise_private.h
  linenoise_hints.c
  linenoise_hints.h
  linenoise_key_binding.c
)
target_include_directories(linenoise 
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE ${PROJECT_BINARY_DIR}
)

file(GLOB headers *.h)
install(FILES ${headers} DESTINATION include/linenoise)
install(TARGETS linenoise ARCHIVE DESTINATION lib)
