cmake_minimum_required(VERSION 3.22)

project(SNS)

set( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin )

add_subdirectory(./proto)
add_subdirectory(./FSWrapper)
add_subdirectory(./coordinator)
add_subdirectory(./server)
add_subdirectory(./client)
add_subdirectory(./test)