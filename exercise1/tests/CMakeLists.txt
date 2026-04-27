cmake_minimum_required(VERSION 3.14)
project(my_project)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)


if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
  cmake_policy(SET CMP0135 NEW)
endif()

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)


enable_testing()

add_compile_options(-Wall -Wextra -Werror -Wpedantic -pedantic -g)

# Debug: Enable address and undefinined behaviour sanitization
# add_compile_options(-Wall -Wextra -Werror -Wpedantic -pedantic -g -fsanitize=address -fsanitize=undefined)
# add_link_options(-fsanitize=address -fsanitize=undefined)

# Add test executables
add_executable(get_from_bitmap_test get_from_bitmap_test.cc ../src/balloc.c)
add_executable(return_to_bitmap_test return_to_bitmap_test.cc ../src/balloc.c)
add_executable(os_allocator_test os_allocator_test.cc ../src/balloc.c)
add_executable(user_api_test user_api_test.cc ../src/balloc.c)
add_executable(extended_test extended_test.cc ../src/balloc.c)

# Link with GTest
target_link_libraries(get_from_bitmap_test GTest::gtest_main)
target_link_libraries(return_to_bitmap_test GTest::gtest_main)
target_link_libraries(os_allocator_test GTest::gtest_main)
target_link_libraries(user_api_test GTest::gtest_main)
target_link_libraries(extended_test GTest::gtest_main)


include(GoogleTest)
gtest_discover_tests(get_from_bitmap_test)
gtest_discover_tests(return_to_bitmap_test)
gtest_discover_tests(os_allocator_test)
gtest_discover_tests(user_api_test)
gtest_discover_tests(extended_test)

# Install rules
install(TARGETS 
  get_from_bitmap_test
  return_to_bitmap_test
  os_allocator_test
  user_api_test  
  extended_test
  RUNTIME DESTINATION bin/tests
)
