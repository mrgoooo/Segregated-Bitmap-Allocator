cmake_minimum_required(VERSION 3.18)

project(BlockAlloc)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(include)
include_directories(src)
include_directories(bench)

add_compile_options(-Wall -Wextra -Werror -Wpedantic -pedantic -mpopcnt -mlzcnt -msse -g -march=native )

add_executable(BitmapAlloc src/main.c src/balloc.c)

add_executable(BitmapAllocOpt src/main.c src/balloc.c)
target_compile_options(BitmapAllocOpt PRIVATE -O3 -march=native)


add_subdirectory(tests)
add_subdirectory(bench)

# Install rules
install(TARGETS 
  BitmapAlloc
  BitmapAllocOpt
  RUNTIME DESTINATION bin
)
