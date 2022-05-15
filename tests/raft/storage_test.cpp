#include <gtest/gtest.h>
#include <memory>

#include "global_ctx_manager.h"
#include "log.grpc.pb.h"
#include "logger.h"
#include "storage.h"

namespace raft {

class SetMetadataTest : public ::testing::Test {
protected:
  void SetUp() override {
    Logger::SetLevel(Logger::LogLevel::DEBUG);
    log = std::make_unique<PersistedLog>(
        std::filesystem::current_path().string() + "/test_log/");
  }

  void TearDown() override {
    std::filesystem::remove_all(std::filesystem::current_path().string() + "/test_log");
  }

  std::unique_ptr<PersistedLog> log;
  protocol::log::LogMetadata test_metadata;
};

class AppendTest : public ::testing::Test {
protected:
  void SetUp(const std::size_t count, const std::size_t max_file_size = 1024*8) {
    Logger::SetLevel(Logger::LogLevel::DEBUG);
    log = std::make_unique<PersistedLog>(
        std::filesystem::current_path().string() + "/test_log/",
        max_file_size);
    entry_count = count;

    for (int i = 0; i < entry_count; i++) {
      protocol::log::LogEntry new_entry;
      new_entry.set_term(i);
      new_entry.set_data("test" + std::to_string(i));
      entries.push_back(new_entry);
    }

    log->Append(entries);
  }

  void TearDown() override {
    std::filesystem::remove_all(std::filesystem::current_path().string() + "/test_log");
  }

  std::unique_ptr<PersistedLog> log;
  std::vector<protocol::log::LogEntry> entries;
  std::size_t entry_count;
};

class RestoreStateTest : public ::testing::Test {
protected:
  void SetUp(const std::size_t count, const std::size_t max_file_size = 1024*8) {
    Logger::SetLevel(Logger::LogLevel::DEBUG);
    log = std::make_unique<PersistedLog>(
        std::filesystem::current_path().string() + "/test_log/",
        max_file_size);
    entry_count = count;
    for (int i = 0; i < entry_count; i++) {
      protocol::log::LogEntry new_entry;
      new_entry.set_term(i);
      new_entry.set_data("test" + std::to_string(i));
      entries.push_back(new_entry);
    }
    log->Append(entries);
    log.reset(new PersistedLog(
          std::filesystem::current_path().string() + "/test_log/",
          max_file_size));
  }

  void TearDown() override {
    std::filesystem::remove_all(std::filesystem::current_path().string() + "/test_log");
  }

  std::unique_ptr<PersistedLog> log;
  std::vector<protocol::log::LogEntry> entries;
  std::size_t entry_count;
};

TEST_F(SetMetadataTest, HandlesNoDataOnDisk) {
  auto [_, valid] = log->Metadata();

  ASSERT_FALSE(valid);
}

TEST_F(SetMetadataTest, ValidateMetadataOnDisk) {
  test_metadata.set_term(4);
  test_metadata.set_vote("peer1");
  log->SetMetadata(test_metadata);

  // Verify that when data is restored from disk that the metadata has not changed
  auto [result_metadata, valid] = log->Metadata();

  ASSERT_TRUE(valid);
  EXPECT_EQ(test_metadata.term(), result_metadata.term());
  EXPECT_EQ(test_metadata.vote(), result_metadata.vote());
}

TEST_F(AppendTest, ValidateSingleFileEntries) {
  SetUp(3);

  // Verify that entries in persisted log map match initially appended entries
  ASSERT_EQ(log->LogSize(), entry_count);
  for (int i = 0; i < entry_count; i++) {
    EXPECT_EQ(log->Entry(i).term(), entries[i].term());
    EXPECT_EQ(log->Entry(i).data(), entries[i].data());
  }
  EXPECT_THROW(log->Entry(3), std::out_of_range);

  // Verify that data remains valid when querying a slice of log entries
  auto expected_slice = std::vector<protocol::log::LogEntry>(entries.begin() + 1, entries.begin() + 3);
  auto result_slice = log->Entries(1, 3);
  ASSERT_EQ(expected_slice.size(), result_slice.size());
  for (int i = 0; i < expected_slice.size(); i++) {
    EXPECT_EQ(result_slice[i].term(), expected_slice[i].term());
    EXPECT_EQ(result_slice[i].data(), expected_slice[i].data());
  }
}

TEST_F(AppendTest, ValidateMultipleFileEntries) {
  SetUp(8, 25);
  
  // Verify that entries in persisted log map match initially appended entries
  ASSERT_EQ(log->LogSize(), entry_count);
  for (int i = 0; i < entry_count; i++) {
    EXPECT_EQ(log->Entry(i).term(), entries[i].term());
    EXPECT_EQ(log->Entry(i).data(), entries[i].data());
  }
  EXPECT_THROW(log->Entry(8), std::out_of_range);

  // Verify that data remains valid when querying a slice of log entries
  auto expected_slice = std::vector<protocol::log::LogEntry>(entries.begin() + 2, entries.begin() + 7);
  auto result_slice = log->Entries(2, 7);
  ASSERT_EQ(expected_slice.size(), result_slice.size());
  for (int i = 0; i < expected_slice.size(); i++) {
    EXPECT_EQ(result_slice[i].term(), expected_slice[i].term());
    EXPECT_EQ(result_slice[i].data(), expected_slice[i].data());
  }
}

TEST_F(RestoreStateTest, HandlesSingleFilePersistence) {
  SetUp(3);

  // Verify that entries persisted to disk have not been altered since insertion
  ASSERT_EQ(log->LogSize(), entries.size());
  for (int i = 0; i < entry_count; i++) {
    EXPECT_EQ(log->Entry(i).term(), entries[i].term());
    EXPECT_EQ(log->Entry(i).data(), entries[i].data());
  }

  // Verify that data remains valid when querying a slice of log entries after restart
  auto expected_slice = std::vector<protocol::log::LogEntry>(entries.begin() + 1, entries.begin() + 3);
  auto result_slice = log->Entries(1, 3);
  ASSERT_EQ(expected_slice.size(), result_slice.size());
  for (int i = 0; i < expected_slice.size(); i++) {
    EXPECT_EQ(result_slice[i].term(), expected_slice[i].term());
    EXPECT_EQ(result_slice[i].data(), expected_slice[i].data());
  }
}

TEST_F(RestoreStateTest, HandlesMultipleFilePersistence) {
  SetUp(8, 25);

  // Verify that entries persisted to disk have not been altered since insertion
  ASSERT_EQ(log->LogSize(), entries.size());
  for (int i = 0; i < entry_count; i++) {
    EXPECT_EQ(log->Entry(i).term(), entries[i].term());
    EXPECT_EQ(log->Entry(i).data(), entries[i].data());
  }

  // Verify that data remains valid when querying a slice of log entries
  auto expected_slice = std::vector<protocol::log::LogEntry>(entries.begin() + 4, entries.begin() + 8);
  auto result_slice = log->Entries(4, 8);
  ASSERT_EQ(expected_slice.size(), result_slice.size());
  for (int i = 0; i < expected_slice.size(); i++) {
    EXPECT_EQ(result_slice[i].term(), expected_slice[i].term());
    EXPECT_EQ(result_slice[i].data(), expected_slice[i].data());
  }
}

}

