cmake_minimum_required(VERSION 3.22)

project(FSWrapper)

FILE(GLOB_RECURSE SOURCES "./FSWrapper/*.cpp")
FILE(GLOB_RECURSE HEADERS "./FSWrapper/*.h")

# Add library
add_library(FSWrapper ${SOURCES} ${HEADERS})
target_include_directories(FSWrapper PUBLIC ${CMAKE_BINARY_DIR})