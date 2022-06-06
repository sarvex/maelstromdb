#ifndef STORAGE_H 
#define STORAGE_H

#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/util/delimited_message_util.h>
#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
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

  virtual std::tuple<protocol::log::LogMetadata, bool> Metadata() const = 0;
  virtual void SetMetadata(const protocol::log::LogMetadata& metadata) = 0;

  virtual int LogSize() const = 0;

  virtual int LastLogIndex() const = 0;
  virtual int LastLogTerm() const = 0;

  virtual protocol::log::LogEntry Entry(const int idx) const = 0;
  virtual std::vector<protocol::log::LogEntry> Entries(int start, int end) const = 0;

  virtual void Append(const std::vector<protocol::log::LogEntry>& new_entries) = 0;

  virtual void TruncateSuffix(const int removal_index) = 0;

protected:
  int m_log_size;
  std::tuple<protocol::log::LogMetadata, bool> m_metadata;
};

class PersistedLog : public Log {
public:
  PersistedLog(
      const std::string& parent_dir,
      const int max_file_size = 1024*8);

  std::tuple<protocol::log::LogMetadata, bool> Metadata() const override;
  void SetMetadata(const protocol::log::LogMetadata& metadata) override;

  int LogSize() const override;

  int LastLogIndex() const override;
  int LastLogTerm() const override;

  protocol::log::LogEntry Entry(const int idx) const override;
  std::vector<protocol::log::LogEntry> Entries(int start, int end) const override;

  void Append(const std::vector<protocol::log::LogEntry>& new_entries) override;

  void TruncateSuffix(const int removal_index) override;

  class Page {
  public:
    struct Record {
      Record(int offset, protocol::log::LogEntry entry);

      protocol::log::LogEntry entry;
      int offset;
    };

  public:
    Page(
        const int start,
        const std::string& dir,
        const bool is_open,
        const int max_file_size);

    Page(const Page&& page);
    Page& operator=(const Page&& page);

    void Close();

    int RemainingSpace() const;

    bool WriteLogEntry(std::fstream& file, const protocol::log::LogEntry& new_entry);

    void TruncateSuffix(int removal_index);

  private:
    std::string ClosedFilename() const;

  public:
    bool is_open;
    int byte_offset;
    int start_index;
    int end_index;
    std::string dir;
    std::string filename;
    std::vector<Record> log_entries;
    const int max_file_size;
  };

private:
  std::vector<std::string> ListDirectoryContents(const std::string& dir);

  bool Empty(const std::string& path) const;

  bool IsFileOpen(const std::string& filename) const;

  void RestoreState();

  void PersistMetadata(const std::string& metadata_path);
  void PersistLogEntries(const std::vector<protocol::log::LogEntry>& new_entries);

  void LoadMetadata(const std::string& metadata_path);
  std::vector<Page::Record> LoadLogEntries(const std::string& log_path) const;

  void CreateOpenFile();

private:
  std::shared_ptr<Page> m_open_page;
  std::shared_ptr<core::AsyncExecutor> m_file_executor;
  std::string m_dir;
  std::map<int, std::shared_ptr<Page>> m_log_indices;
  const int m_max_file_size;
};

}

#endif

