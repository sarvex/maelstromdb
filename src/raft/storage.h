#ifndef STORAGE_H 
#define STORAGE_H

#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/util/delimited_message_util.h>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include "async_executor.h"
#include "logger.h"
#include "log.grpc.pb.h"

namespace raft {

class GlobalCtxManager;

class Log {
public:
  Log();
  virtual ~Log();

  virtual void Append(const std::vector<protocol::log::LogEntry>& new_entries) = 0;
  virtual void SetMetadata(const protocol::log::LogMetadata& metadata) = 0;

  virtual ssize_t LastLogIndex() const = 0;
  virtual ssize_t LastLogTerm() const = 0;

  virtual protocol::log::LogEntry Entry(const std::size_t idx) const = 0;
  virtual std::vector<protocol::log::LogEntry> Entries(std::size_t start, std::size_t end) const = 0;

  virtual std::size_t LogSize() const = 0;

  virtual std::tuple<protocol::log::LogMetadata, bool> RestoreState() = 0;

protected:
  protocol::log::LogMetadata m_metadata;
  std::size_t m_log_size;
};

class PersistedLog : public Log {
public:
  PersistedLog(
      const std::string& parent_dir,
      const std::size_t max_file_size = 1024*8);

  void Append(const std::vector<protocol::log::LogEntry>& new_entries) override;
  void SetMetadata(const protocol::log::LogMetadata& metadata) override;

  ssize_t LastLogIndex() const override;
  ssize_t LastLogTerm() const override;

  protocol::log::LogEntry Entry(const std::size_t idx) const override;
  std::vector<protocol::log::LogEntry> Entries(std::size_t start, std::size_t end) const override;

  std::size_t LogSize() const override;

  std::tuple<protocol::log::LogMetadata, bool> RestoreState() override;

  class Page {
  public:
    Page(
        const std::size_t start,
        const std::string& dir,
        const std::size_t max_file_size);

    Page(const Page& page);
    Page(const Page&& page);
    Page& operator=(const Page&& page);

    void Close();

    std::size_t RemainingSpace() const;

    bool WriteLogEntry(std::fstream& file, const protocol::log::LogEntry& new_entry);

  public:
    bool is_open;
    std::size_t byte_offset;
    std::size_t start_index;
    std::size_t end_index;
    std::string filename;
    std::vector<protocol::log::LogEntry> log_entries;
    const std::size_t max_file_size;
  };

  std::vector<std::string> ListDirectoryContents(const std::string& dir);

  bool Empty(const std::string& path) const;

  void PersistMetadata(const std::string& metadata_path);
  void PersistLogEntries(const std::vector<protocol::log::LogEntry>& new_entries);

  protocol::log::LogMetadata LoadMetadata(const std::string& metadata_path) const;
  Page LoadLogEntries(const std::string& log_path) const;

  void CreatePage();

private:
  Page m_open_page;
  std::shared_ptr<core::AsyncExecutor> m_file_executor;
  std::string m_dir;
  std::map<std::size_t, Page*> m_log_indices;
  const std::size_t m_max_file_size;
};

}

#endif

