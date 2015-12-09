cmake_minimum_required(VERSION 2.8.7)
project(libmill C)

include(CheckSymbolExists)
include(CheckFunctionExists)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")

# static and shared libs
file(GLOB sources *.c dns/*.c)
set_source_files_properties(dns/dns.c PROPERTIES COMPILE_FLAGS -std=c99)
add_library(mill STATIC ${sources})
add_library(mill_s SHARED ${sources})
set_target_properties(mill_s PROPERTIES OUTPUT_NAME mill)

# check and enable rt if available
list(APPEND CMAKE_REQUIRED_LIBRARIES rt)
check_symbol_exists(clock_gettime time.h HAVE_CLOCK_GETTIME)
if(HAVE_CLOCK_GETTIME)
  target_link_libraries(mill rt)
  target_link_libraries(mill_s rt)
endif()

set(CMAKE_REQUIRED_LIBRARIES )

# check and enable stack guard and dns if available
list(APPEND CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)

set(CMAKE_REQUIRED_LIBRARIES )
set(CMAKE_REQUIRED_DEFINITIONS )

check_function_exists(mprotect HAVE_MPROTECT)
if(HAVE_MPROTECT)
  add_definitions(-DHAVE_MPROTECT)
endif()

check_function_exists(posix_memalign HAVE_POSIX_MEMALIGN)
if(HAVE_POSIX_MEMALIGN)
  add_definitions(-DHAVE_POSIX_MEMALIGN)
endif()

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
