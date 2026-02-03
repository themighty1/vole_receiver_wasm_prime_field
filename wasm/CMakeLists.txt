cmake_minimum_required(VERSION 3.13)
project(vole_wasm)

set(CMAKE_CXX_STANDARD 14)

if(EMSCRIPTEN)
    # Enable WASM SIMD + SSE intrinsics (Emscripten translates SSE to WASM SIMD)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -msimd128 -msse2 -msse4.1")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -msimd128 -msse2 -msse4.1")
endif()

# Include paths
include_directories(${CMAKE_SOURCE_DIR}/..)
include_directories(${CMAKE_SOURCE_DIR}/../emp-zk/emp-vole)

# Use portable mode (no emp-tool dependency)
add_definitions(-DEMP_PORTABLE)

# BLAKE3 sources (with SSE SIMD)
set(BLAKE3_SOURCES
    ${CMAKE_SOURCE_DIR}/../emp-zk/emp-vole/blake3.c
    ${CMAKE_SOURCE_DIR}/../emp-zk/emp-vole/blake3_dispatch.c
    ${CMAKE_SOURCE_DIR}/../emp-zk/emp-vole/blake3_portable.c
    ${CMAKE_SOURCE_DIR}/../emp-zk/emp-vole/blake3_sse2.c
    ${CMAKE_SOURCE_DIR}/../emp-zk/emp-vole/blake3_sse41.c
)

# Disable AVX (no WASM support)
set_source_files_properties(${BLAKE3_SOURCES} PROPERTIES
    COMPILE_DEFINITIONS "BLAKE3_NO_AVX2;BLAKE3_NO_AVX512;BLAKE3_NO_NEON"
)

# VOLE Receiver (Bob) - WASM build
add_executable(vole_receiver
    vole_receiver.cpp
    ${BLAKE3_SOURCES}
)

if(EMSCRIPTEN)
    set_target_properties(vole_receiver PROPERTIES
        SUFFIX ".js"
        LINK_FLAGS "-lwebsocket.js -sWASM=1 -sEXPORTED_FUNCTIONS=['_main','_vole_run'] -sEXPORTED_RUNTIME_METHODS=['ccall','cwrap'] -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=512MB -sMAXIMUM_MEMORY=4GB -sASYNCIFY -sASYNCIFY_STACK_SIZE=131072"
    )
endif()
