find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBBITCOIN REQUIRED IMPORTED_TARGET libbitcoin)

add_library(bitcoin_merkle_core merkle.cpp)
target_include_directories(bitcoin_merkle_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(bitcoin_merkle_core PUBLIC PkgConfig::LIBBITCOIN)

add_executable(bitcoin_merkle main.cpp)
target_link_libraries(bitcoin_merkle PRIVATE bitcoin_merkle_core chain_core)
