
cmake_minimum_required(VERSION 3.14)
project(my_project)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_BUILD_TYPE ReleaseWithDebInfo)
string(REPLACE "-DNDEBUG" "" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
string(REPLACE "-DNDEBUG" "" CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}")
#set(CMAKE_VERBOSE_MAKEFILE TRUE)

if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
  cmake_policy(SET CMP0135 NEW)
endif()


include(FetchContent)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
FetchContent_MakeAvailable(googletest)


FetchContent_Declare(
  googlebench
  GIT_REPOSITORY https://github.com/google/benchmark.git
  GIT_TAG afa23b7699c17f1e26c88cbf95257b20d78d6247
)
FetchContent_MakeAvailable(googlebench)

add_compile_options(-Wall -Wextra -Werror -Wpedantic -pedantic -g -O3 -march=native)

function(googlebench_file name)
	add_executable(${name} ../src/balloc.c ${ARGN})
	target_link_libraries(${name} benchmark::benchmark_main m)
endfunction()

googlebench_file(fixed_bench fixed_bench.cc)
googlebench_file(paged_bench paged_bench.cc)
googlebench_file(flexible_bench flexible_bench.cc)
googlebench_file(preparation_bench preparation_bench.cc)
 
# Install rules
install(TARGETS 
  fixed_bench
  paged_bench
  flexible_bench
  preparation_bench
  RUNTIME DESTINATION bin/bench
)
