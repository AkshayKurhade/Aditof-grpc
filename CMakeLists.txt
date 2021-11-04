
cmake_minimum_required(VERSION 3.5.1)

project(TimeOfFlight CPP)

include(common.cmake)
find_package(aditof 1.0.0 REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE aditof::aditof)


# Proto file
get_filename_component(proto_file "buffer.proto" ABSOLUTE)
get_filename_component(proto_file_path "${proto_file}" PATH)

# Generated sources
set(tof_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/buffer.pb.cc")
set(tof_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/buffer.pb.h")
set(tof_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/buffer.grpc.pb.cc")
set(tof_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/buffer.grpc.pb.h")
add_custom_command(
      OUTPUT "${tof_proto_srcs}" "${tof_proto_hdrs}" "${tof_grpc_srcs}" "${tof_grpc_hdrs}"
      COMMAND ${_PROTOBUF_PROTOC}
      ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
        --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
        -I "${proto_file_path}"
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        "${proto_file}"
      DEPENDS "${proto_file}")

# Include generated *.pb.h files
include_directories("${CMAKE_CURRENT_BINARY_DIR}")

# rg_grpc_proto
add_library(tof_grpc_proto
  ${tof_grpc_srcs}
  ${tof_grpc_hdrs}
  ${tof_proto_srcs}
  ${tof_proto_hdrs})
target_link_libraries(tof_grpc_proto
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF})

# route_guide_helper
#add_library(route_guide_helper
# "helper.h"
# "helper.cc")
target_link_libraries(tof_grpc_proto
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF})




add_executable(${PROJECT_NAME} grpc_server.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE aditof ${Protobuf_LIBRARIES} ${LIBWEBSOCKETS_LIBRARIES})

target_include_directories(${PROJECT_NAME} PRIVATE ${Protobuf_INCLUDE_DIRS} ${GENERATED_PROTO_FILES_DIR} ${LIBWEBSOCKETS_INCLUDE_DIRS})

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 11)

# Targets route_guide_(client|server)
foreach(_target
  grpc_server grpc_client)
  add_executable(${_target}
    "${_target}.cc")
  target_link_libraries(${_target}
    tof_grpc_proto
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF})
endforeach()
