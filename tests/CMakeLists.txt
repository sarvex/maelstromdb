add_executable(
  raft_test
  unit/raft/consensus_module_test.cpp
  unit/raft/storage_test.cpp
  unit/raft/session_cache_test.cpp)
target_link_libraries(raft_test
  PRIVATE
  gmock
  gtest
  gtest_main
  maelstromdb_lib)

add_executable(
  raft_bench
  bench/test.cpp)
target_link_libraries(raft_bench
  PRIVATE
  benchmark
  maelstromdb_lib)

include(GoogleTest)
gtest_discover_tests(raft_test)

