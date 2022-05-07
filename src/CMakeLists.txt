foreach(target raft log)
  get_filename_component(proto "../src/protos/${target}.proto" ABSOLUTE)
  get_filename_component(proto_path "${proto}" PATH)
  set(proto_hdrs "${CMAKE_BINARY_DIR}/protogen/${target}.pb.h")
  set(proto_srcs "${CMAKE_BINARY_DIR}/protogen/${target}.pb.cc")
  set(grpc_hdrs "${CMAKE_BINARY_DIR}/protogen/${target}.grpc.pb.h")
  set(grpc_srcs "${CMAKE_BINARY_DIR}/protogen/${target}.grpc.pb.cc")

  add_custom_command(
    OUTPUT "${proto_srcs}" "${proto_hdrs}" "${grpc_srcs}" "${grpc_hdrs}"
    COMMAND ${_PROTOBUF_PROTOC}
    ARGS --grpc_out "${CMAKE_BINARY_DIR}/protogen"
    --cpp_out "${CMAKE_BINARY_DIR}/protogen"
      -I "${proto_path}"
      --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
      "${proto}"
    DEPENDS "${proto}")

  add_library("grpc_${target}_proto"
    ${grpc_srcs}
    ${grpc_hdrs}
    ${proto_srcs}
    ${proto_hdrs})

  target_link_libraries("grpc_${target}_proto"
    grpc++_reflection
    grpc++
    libprotoc
    libprotobuf)
endforeach()

add_executable(maelstromdb
  raft/concensus_module.cpp
  raft/global_ctx_manager.cpp
  raft/raft_client.cpp
  raft/raft_server.cpp
  raft/snapshot.cpp
  core/async_executor.cpp
  core/timer.cpp
  utils/logger.cpp
  raft_node.cpp)
target_link_libraries(maelstromdb
  grpc_raft_proto
  grpc_log_proto)
target_include_directories(maelstromdb
  PRIVATE
  core
  engine
  protos
  raft
  utils
  "${CMAKE_BINARY_DIR}/protogen")

