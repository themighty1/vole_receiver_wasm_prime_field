#Testing macro
macro (add_test_executable_with_lib folder _name)
	add_executable(test_${folder}_${_name} "${folder}/${_name}.cpp")
	target_link_libraries(test_${folder}_${_name} ${EMP-OT_LIBRARIES} emp-zk)
endmacro()

# BLAKE3 portable library (no SIMD)
add_library(blake3_portable STATIC
    ${CMAKE_SOURCE_DIR}/emp-zk/emp-vole/blake3.c
    ${CMAKE_SOURCE_DIR}/emp-zk/emp-vole/blake3_dispatch.c
    ${CMAKE_SOURCE_DIR}/emp-zk/emp-vole/blake3_portable.c
)
target_compile_definitions(blake3_portable PRIVATE
    BLAKE3_NO_SSE2
    BLAKE3_NO_SSE41
    BLAKE3_NO_AVX2
    BLAKE3_NO_AVX512
    BLAKE3_NO_NEON
)

# Mock test with BLAKE3 (no OpenSSL dependency)
macro (add_test_executable_mock folder _name)
    add_executable(test_${folder}_${_name} "${folder}/${_name}.cpp")
    target_link_libraries(test_${folder}_${_name} ${EMP-OT_LIBRARIES} emp-zk blake3_portable)
endmacro()

macro (add_test_case folder _name)
	add_test_executable_with_lib(${folder} ${_name})
  	add_test(NAME ${folder}_${_name} COMMAND "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_${folder}_${_name}" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/")
endmacro()

macro (add_test_case_with_run folder _name)
	add_test_executable_with_lib(${folder} ${_name})
	add_test(NAME ${folder}_${_name} COMMAND "./run" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_${folder}_${_name}" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/")

endmacro()

# Test cases
add_test_case_with_run(bool example)
add_test_case_with_run(bool ostriple)
add_test_case_with_run(bool bool_io)
add_test_case_with_run(bool memory_scalability)
add_test_case_with_run(bool input_scalability)
add_test_case_with_run(bool circuit_scalability)
add_test_case_with_run(bool sha256)
add_test_case_with_run(bool polynomial)
add_test_case_with_run(bool inner_prdt)
add_test_case_with_run(bool lowmc)

add_test_case_with_run(arith abconversion)
add_test_case_with_run(arith zk_proof)
add_test_case_with_run(arith ostriple)
add_test_case_with_run(arith input_scalability)
add_test_case_with_run(arith circuit_scalability)
add_test_case_with_run(arith polynomial)
add_test_case_with_run(arith inner_prdt)
add_test_case_with_run(arith matrix_mul)
add_test_case_with_run(arith sis)

add_test_case_with_run(vole vole_triple)
add_test_case_with_run(vole vole_triple_1t)

# Mock test with BLAKE3
add_test_executable_mock(vole vole_triple_mock)

# Single-process WASM-style benchmark (both parties in same thread)
add_executable(test_vole_wasm_bench "${CMAKE_SOURCE_DIR}/wasm/vole_bench_wasm.cpp")
target_link_libraries(test_vole_wasm_bench ${EMP-OT_LIBRARIES} emp-zk blake3_portable)

# VOLE server (sender/Alice) - native
add_executable(vole_server "${CMAKE_SOURCE_DIR}/wasm/vole_server.cpp")
target_link_libraries(vole_server ${EMP-OT_LIBRARIES} emp-zk blake3_portable)

# VOLE client (receiver/Bob) - native version for testing
add_executable(vole_client "${CMAKE_SOURCE_DIR}/wasm/vole_client.cpp")
target_link_libraries(vole_client ${EMP-OT_LIBRARIES} emp-zk blake3_portable)

add_test(NAME vole_vole_triple_mock COMMAND "./run" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test_vole_vole_triple_mock" WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/")
add_test_case_with_run(vole cope)
add_test_case_with_run(vole base_svole)
add_test_case_with_run(vole lpn)
add_test_case_with_run(vole vole_f2k_triple)

add_test_case_with_run(ram ram_test)
add_test_case_with_run(ram ram_ext_test)
add_test_case_with_run(ram ro_ram_test)
