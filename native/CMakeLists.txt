cmake_minimum_required(VERSION 3.13)
project(vole_native)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -march=native")

include_directories(${CMAKE_SOURCE_DIR}/..)
include_directories(${CMAKE_SOURCE_DIR}/../emp-zk/emp-vole)

add_definitions(-DEMP_PORTABLE)

# BLAKE3 sources (with SIMD)
set(BLAKE3_SOURCES
    ${CMAKE_SOURCE_DIR}/../emp-zk/emp-vole/blake3.c
    ${CMAKE_SOURCE_DIR}/../emp-zk/emp-vole/blake3_dispatch.c
    ${CMAKE_SOURCE_DIR}/../emp-zk/emp-vole/blake3_portable.c
    ${CMAKE_SOURCE_DIR}/../emp-zk/emp-vole/blake3_sse2.c
    ${CMAKE_SOURCE_DIR}/../emp-zk/emp-vole/blake3_sse41.c
)

# Enable SSE2/SSE41, disable AVX2/AVX512 (don't have those source files)
set_source_files_properties(${BLAKE3_SOURCES} PROPERTIES
    COMPILE_DEFINITIONS "BLAKE3_NO_AVX2;BLAKE3_NO_AVX512"
)

# VOLE Sender (Alice)
add_executable(vole_sender vole_sender.cpp ${BLAKE3_SOURCES})

# VOLE Receiver (Bob)
add_executable(vole_receiver vole_receiver.cpp ${BLAKE3_SOURCES})
