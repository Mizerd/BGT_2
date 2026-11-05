cmake_minimum_required(VERSION 3.20)
project(blockchain_assignment VERSION 0.3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_library(chain_core src/custom_hash.cpp src/models.cpp src/chain.cpp)
target_include_directories(chain_core PUBLIC src)

add_executable(blockchain src/main.cpp)
target_link_libraries(blockchain PRIVATE chain_core)

add_executable(blockchain_tests tests/test_blockchain.cpp)
target_link_libraries(blockchain_tests PRIVATE chain_core)
set_target_properties(blockchain_tests PROPERTIES OUTPUT_NAME tests)

enable_testing()
add_test(NAME blockchain_tests COMMAND blockchain_tests)

add_executable(v03_tests tests/test_v03.cpp)
target_link_libraries(v03_tests PRIVATE chain_core)
add_test(NAME v03_tests COMMAND v03_tests)
