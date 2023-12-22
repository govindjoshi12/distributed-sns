cmake_minimum_required(VERSION 3.22)

find_package(protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(glog CONFIG REQUIRED)
find_package(Threads)

# source files
file(GLOB PROTO_FILES "snsproto/*.proto")

# Add library
add_library(snsproto ${PROTO_FILES})
target_link_libraries(snsproto
    PUBLIC
        protobuf::libprotobuf
        gRPC::grpc
        gRPC::grpc++
        glog::glog
)
target_include_directories(snsproto PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

# Compile
get_target_property(grpc_cpp_plugin_location gRPC::grpc_cpp_plugin LOCATION)
protobuf_generate(TARGET snsproto LANGUAGE cpp)
protobuf_generate(TARGET snsproto LANGUAGE grpc GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc PLUGIN "protoc-gen-grpc=${grpc_cpp_plugin_location}")