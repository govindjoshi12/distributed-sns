cmake_minimum_required(VERSION 3.22)

project(server)

FILE(GLOB_RECURSE SOURCES "./src/*.cpp")
FILE(GLOB_RECURSE SOURCES_CC "./src/*.cc")
FILE(GLOB_RECURSE HEADERS "./src/*.h")

# Target
# proto_lib is name of library built by cmake file in proto dir
add_executable(${PROJECT_NAME} ${SOURCES} ${SOURCES_CC} ${HEADERS})
target_link_libraries(${PROJECT_NAME} PRIVATE snsproto FSWrapper)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/FSWrapper)
