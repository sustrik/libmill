cmake_minimum_required(VERSION 3.0)
project(libmill C)
file(GLOB sources *.c)
add_library(mill STATIC ${sources})
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wpedantic -Wextra")

# tests
file(GLOB test_files tests/*.c)
foreach(test_file IN LISTS test_files)
  get_filename_component(test_name ${test_file} NAME_WE)
  add_executable(test_${test_name} ${test_file})
  target_link_libraries(test_${test_name} mill)
endforeach()

