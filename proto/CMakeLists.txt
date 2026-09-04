find_package(Protobuf QUIET CONFIG)
if(NOT Protobuf_FOUND)
    find_package(Protobuf REQUIRED)
endif()
find_package(gRPC CONFIG REQUIRED)

set(PROTO_FILE ${CMAKE_CURRENT_SOURCE_DIR}/edgescope.proto)
set(PROTO_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${PROTO_GEN_DIR})

set(PROTO_SRCS
    ${PROTO_GEN_DIR}/edgescope.pb.cc
    ${PROTO_GEN_DIR}/edgescope.grpc.pb.cc
)
set(PROTO_HDRS
    ${PROTO_GEN_DIR}/edgescope.pb.h
    ${PROTO_GEN_DIR}/edgescope.grpc.pb.h
)

add_custom_command(
    OUTPUT ${PROTO_SRCS} ${PROTO_HDRS}
    COMMAND protobuf::protoc
    ARGS
        --cpp_out=${PROTO_GEN_DIR}
        --grpc_out=${PROTO_GEN_DIR}
        -I${CMAKE_CURRENT_SOURCE_DIR}
        --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
        ${PROTO_FILE}
    DEPENDS ${PROTO_FILE}
    COMMENT "Generating protobuf/gRPC sources from edgescope.proto"
    VERBATIM
)

add_custom_target(edgescope_proto_gen
    DEPENDS ${PROTO_SRCS} ${PROTO_HDRS}
)

add_library(edgescope_proto STATIC ${PROTO_SRCS})
add_dependencies(edgescope_proto edgescope_proto_gen)

target_include_directories(edgescope_proto PUBLIC ${PROTO_GEN_DIR})
target_link_libraries(edgescope_proto PUBLIC
    protobuf::libprotobuf
    gRPC::grpc++
)
