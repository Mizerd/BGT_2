find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBBITCOIN REQUIRED IMPORTED_TARGET libbitcoin)

add_library(bitcoin_merkle_core merkle.cpp)
target_include_directories(bitcoin_merkle_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(bitcoin_merkle_core PUBLIC PkgConfig::LIBBITCOIN)

add_executable(bitcoin_merkle main.cpp)
target_link_libraries(bitcoin_merkle PRIVATE bitcoin_merkle_core chain_core)

add_executable(bitcoin_merkle_tests merkle_tests.cpp)
target_link_libraries(bitcoin_merkle_tests PRIVATE bitcoin_merkle_core)
add_test(NAME bitcoin_merkle_tests COMMAND bitcoin_merkle_tests
         ${CMAKE_SOURCE_DIR}/assignment3/fixtures/block100000-txids.txt
         ${CMAKE_SOURCE_DIR}/assignment3/evidence/lab/block-txids.txt
         ${CMAKE_SOURCE_DIR}/assignment3/evidence/lab/merkleroot.txt)

set(A3_PYTHON ${CMAKE_SOURCE_DIR}/assignment3/.venv/bin/python)
if(EXISTS ${A3_PYTHON})
  add_test(NAME bitcoin_analysis_offline COMMAND ${A3_PYTHON} -m unittest discover
           -s ${CMAKE_SOURCE_DIR}/assignment3/tests -p test_offline.py -v)
endif()
