add_executable(
  raft_test
  raft/concensus_module_test.cpp
  raft/storage_test.cpp)
target_link_libraries(raft_test
  PRIVATE
  gmock
  gtest
  gtest_main
  maelstromdb_lib)

include(GoogleTest)
gtest_discover_tests(raft_test)

