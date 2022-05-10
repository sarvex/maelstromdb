#ifndef STORAGE_H 
#define STORAGE_H

#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/util/delimited_message_util.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "logger.h"
#include "log.grpc.pb.h"

namespace raft {

class GlobalCtxManager;

class Log {
public:
  Log(GlobalCtxManager& ctx);
  virtual ~Log();

  virtual void Append(const std::vector<protocol::log::LogEntry>& new_entries) = 0;
  virtual void SetMetadata(const protocol::log::LogMetadata& metadata) = 0;

  virtual bool Empty() const = 0;

  virtual ssize_t LastLogIndex() const = 0;
  virtual ssize_t LastLogTerm() const = 0;

  virtual protocol::log::LogEntry Entry(const std::size_t idx) const = 0;
  virtual std::vector<protocol::log::LogEntry> Entries() const = 0;

  virtual std::size_t LogSize() const = 0;

protected:
  GlobalCtxManager& m_ctx;
  protocol::log::LogMetadata m_metadata;
  std::vector<protocol::log::LogEntry> m_entries;
};

class PersistedLog : public Log {
public:
  PersistedLog(
      GlobalCtxManager& ctx,
      const std::string& parent_dir,
      const std::size_t file_size = 1024*8);

  void Append(const std::vector<protocol::log::LogEntry>& new_entries) override;
  void SetMetadata(const protocol::log::LogMetadata& metadata) override;

  bool Empty() const override;

  ssize_t LastLogIndex() const override;
  ssize_t LastLogTerm() const override;

  protocol::log::LogEntry Entry(const std::size_t idx) const override;
  std::vector<protocol::log::LogEntry> Entries() const override;

  std::size_t LogSize() const override;

private:
  std::tuple<protocol::log::LogMetadata, std::vector<protocol::log::LogEntry>> RestoreState();

  void PersistMetadata();
  void PersistLogEntries(const std::vector<protocol::log::LogEntry>& new_entries);

  // void PrepareNewLog();
  // void CloseLog();

  std::vector<protocol::log::LogEntry> LoadLogEntries() const;
  protocol::log::LogMetadata LoadMetadata() const;

private:
  std::string m_dir;
  std::size_t m_max_file_size;
};

}

#endif

