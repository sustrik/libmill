cmake_minimum_required(VERSION 3.0)
project(libmill C)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wpedantic -Wextra")

# static and shared libs
file(GLOB sources *.c)
add_library(mill STATIC ${sources})
add_library(mill_s SHARED ${sources})
set_target_properties(mill_s PROPERTIES OUTPUT_NAME mill)

# tests
enable_testing()
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/tests)
file(GLOB test_files tests/*.c)
foreach(test_file IN LISTS test_files)
  get_filename_component(test_name ${test_file} NAME_WE)
  add_executable(test_${test_name} ${test_file})
  set_target_properties(test_${test_name} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
    OUTPUT_NAME ${test_name})
  target_link_libraries(test_${test_name} mill)
  add_test(test_${test_name} tests/${test_name})
endforeach()

# perf
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/perf)
file(GLOB perf_files perf/*.c)
foreach(perf_file IN LISTS perf_files)
  get_filename_component(perf_name ${perf_file} NAME_WE)
  add_executable(perf_${perf_name} ${perf_file})
  set_target_properties(perf_${perf_name} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/perf
    OUTPUT_NAME ${perf_name})
  target_link_libraries(perf_${perf_name} mill)
endforeach()

