#include "global_ctx_manager.h"
#include "storage.h"

namespace raft {

Log::Log(GlobalCtxManager& ctx)
  : m_ctx(ctx), m_entries({}), m_metadata() {
}

Log::~Log() {
}

PersistedLog::PersistedLog(
    GlobalCtxManager& ctx,
    const std::string& parent_dir,
    const std::size_t file_size)
  : Log(ctx), m_dir(parent_dir), m_max_file_size(file_size) {
  std::filesystem::create_directories(parent_dir);

  if (!Empty()) {
    RestoreState();
  }
}

ssize_t PersistedLog::LastLogIndex() const {
  return m_entries.size() - 1;
}

ssize_t PersistedLog::LastLogTerm() const {
  if (m_entries.size() > 0) {
    return m_entries[LastLogIndex()].term();
  } else {
    return -1;
  }
}

void PersistedLog::Append(const std::vector<protocol::log::LogEntry>& new_entries) {
  m_entries.insert(m_entries.end(), new_entries.begin(), new_entries.end());
  PersistLogEntries(new_entries);
}

void PersistedLog::SetMetadata(const protocol::log::LogMetadata& metadata) {
  m_metadata = metadata;
  PersistMetadata();
}

bool PersistedLog::Empty() const {
  const std::string metadata_file = m_dir + "metadata";
  const std::string log_file = m_dir + "log";
  std::fstream metadata(metadata_file, std::ios::in | std::ios::binary);
  std::fstream log_entries(log_file, std::ios::in | std::ios::binary);

  return metadata.peek() == std::fstream::traits_type::eof() ||
    log_entries.peek() == std::fstream::traits_type::eof();
}


protocol::log::LogEntry PersistedLog::Entry(const std::size_t idx) const {
  if (idx > LastLogIndex()) {
    throw std::out_of_range("Raft log index out of bounds");
  }
  return m_entries[idx];
}

std::vector<protocol::log::LogEntry> PersistedLog::Entries() const {
  return m_entries;
}

std::size_t PersistedLog::LogSize() const {
  return m_entries.size();
}

void PersistedLog::PersistMetadata() {
  const std::string metadata_file = m_dir + "/metadata";
  std::fstream out(metadata_file, std::ios::out | std::ios::trunc | std::ios::binary);

   bool success = google::protobuf::util::SerializeDelimitedToOstream(m_metadata, &out);
  if (!success) {
    Logger::Error("Unexpected serialization failure when persisting raft metadata to disk");
    throw std::runtime_error("Unable to serialize raft metadata");
  }
  out.flush();
}

std::tuple<protocol::log::LogMetadata, std::vector<protocol::log::LogEntry>> PersistedLog::RestoreState() {
  m_metadata = LoadMetadata();
  m_entries = LoadLogEntries();
  return std::make_tuple(m_metadata, m_entries);
}

void PersistedLog::PersistLogEntries(const std::vector<protocol::log::LogEntry>& new_entries) {
  const std::string log_file = m_dir + "/log";
  std::fstream out(log_file, std::ios::out | std::ios::app | std::ios::binary);

  bool success = true;
  for (auto &entry:new_entries) {
    success = google::protobuf::util::SerializeDelimitedToOstream(entry, &out);

    if (!success) {
      Logger::Error("Unexpected serialization failure when persisting raft log to disk");
      throw std::runtime_error("Unable to serialize raft log");
    }
  }
  out.flush();
}

std::vector<protocol::log::LogEntry> PersistedLog::LoadLogEntries() const {
  const std::string log_file = m_dir + "/log";
  std::fstream in(log_file, std::ios::in | std::ios::binary);

  google::protobuf::io::IstreamInputStream log_stream(&in);
  std::vector<protocol::log::LogEntry> log_entries;
  protocol::log::LogEntry temp_log_entry;
  bool success = true;

  while (success) {
    success = google::protobuf::util::ParseDelimitedFromZeroCopyStream(&temp_log_entry, &log_stream, nullptr);
    if (success) {
      log_entries.push_back(temp_log_entry);
    }
  }

  if (!success) {
    Logger::Error("Unable to restore log entries from disk");
    throw std::runtime_error("Unable to restore log entries");
  }
  Logger::Debug("Restored log entries from disk, size =", log_entries.size());

  return log_entries;
}

protocol::log::LogMetadata PersistedLog::LoadMetadata() const {
  const std::string metadata_file = m_dir + "/metadata";
  std::fstream in(metadata_file, std::ios::in | std::ios::binary);

  google::protobuf::io::IstreamInputStream metadata_stream(&in);
  protocol::log::LogMetadata metadata;
  bool success = google::protobuf::util::ParseDelimitedFromZeroCopyStream(&metadata, &metadata_stream, nullptr);

  if (!success) {
    Logger::Error("Unable to restore metadata from disk");
    throw std::runtime_error("Unable to restore metadata");
  }
  Logger::Debug("Restored metadata from disk, term =", metadata.term(), "vote =", metadata.vote());

  return metadata;
}

}

