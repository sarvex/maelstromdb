#include "global_ctx_manager.h"
#include "storage.h"

namespace raft {

Log::Log()
  : m_metadata(), m_log_size(0) {
}

Log::~Log() {
}

PersistedLog::Page::Page(
    const std::size_t start,
    const std::string& dir,
    const std::size_t max_file_size)
  : byte_offset(0)
  , start_index(start)
  , end_index(start)
  , filename("open-" + std::to_string(start))
  , log_entries({})
  , max_file_size(max_file_size) {
  std::filesystem::create_directories(dir);

  std::string file_path = dir + filename;
  std::fstream out(file_path, std::ios::out | std::ios::binary);
  // std::filesystem::resize_file(file_path, max_file_size);
}

PersistedLog::Page::Page(const Page& page)
  : is_open(page.is_open)
  , byte_offset(page.byte_offset)
  , start_index(page.start_index)
  , end_index(page.end_index)
  , filename(page.filename)
  , log_entries(page.log_entries)
  , max_file_size(page.max_file_size) {
}

PersistedLog::Page::Page(const Page&& page)
  : is_open(page.is_open)
  , byte_offset(page.byte_offset)
  , start_index(page.start_index)
  , end_index(page.end_index)
  , filename(page.filename)
  , log_entries(std::move(page.log_entries))
  , max_file_size(page.max_file_size) {
}

PersistedLog::Page& PersistedLog::Page::operator=(const PersistedLog::Page&& page) {
  if (this != &page) {
    is_open = page.is_open;
    byte_offset = page.byte_offset;
    start_index = page.start_index;
    end_index = page.end_index;
    filename = page.filename;
    log_entries = std::move(page.log_entries);
  }
  return *this;
}

void PersistedLog::Page::Close() {
  if (!is_open) {
    return;
  }
  is_open = false;

  const std::string format = "%020lu-%020lu";
  const auto size = std::snprintf(nullptr, 0, format.c_str(), start_index, end_index);
  const auto buffer = std::make_unique<char[]>(size);
  std::snprintf(buffer.get(), size, format.c_str(), start_index, end_index);
  const std::string closed_filename = std::string(buffer.get(), buffer.get() + size - 1);
  std::filesystem::rename(filename, closed_filename);
  filename = closed_filename;
}

std::size_t PersistedLog::Page::RemainingSpace() const {
  return max_file_size - byte_offset;
}

bool PersistedLog::Page::WriteLogEntry(std::fstream& file, const protocol::log::LogEntry& new_entry) {
    bool success = google::protobuf::util::SerializeDelimitedToOstream(new_entry, &file);
    byte_offset += new_entry.ByteSizeLong();
    end_index++;

    if (success) {
      log_entries.push_back(new_entry);
    }
    return success;
}

PersistedLog::PersistedLog(
    const std::string& parent_dir,
    const std::size_t max_file_size)
  : Log()
  , m_dir(parent_dir)
  , m_file_executor(std::make_shared<core::Strand>())
  , m_open_page(0, parent_dir, max_file_size)
  , m_log_indices()
  , m_max_file_size(max_file_size) {
  m_log_indices.insert({0, &m_open_page});
}

ssize_t PersistedLog::LastLogIndex() const {
  return m_log_size - 1;
}

ssize_t PersistedLog::LastLogTerm() const {
  if (m_log_size > 0) {
    return Entry(LastLogIndex()).term();
  } else {
    return -1;
  }
}

void PersistedLog::CreatePage() {
  if (m_open_page.is_open) {
    m_open_page.Close();
  }

  m_open_page = Page(LogSize(), m_dir, m_max_file_size);
  m_log_indices.insert({LogSize(), &m_open_page});
}

std::vector<std::string> PersistedLog::ListDirectoryContents(const std::string& dir) {
  std::vector<std::string> file_list;
  for (const auto& entry:std::filesystem::directory_iterator(dir)) {
    if (entry.is_regular_file()) {
      file_list.push_back(entry.path().filename());
    }
  }
  return file_list;
}

void PersistedLog::Append(const std::vector<protocol::log::LogEntry>& new_entries) {
  PersistLogEntries(new_entries);
}

void PersistedLog::SetMetadata(const protocol::log::LogMetadata& metadata) {
  const std::string& metadata_path = m_dir + "metadata";
  m_metadata = metadata;
  PersistMetadata(metadata_path);
}

bool PersistedLog::Empty(const std::string& path) const {
  return std::filesystem::file_size(path) == 0;
}

protocol::log::LogEntry PersistedLog::Entry(const std::size_t idx) const {
  if (idx > LastLogIndex()) {
    Logger::Error("Attempt to access log entry out of bounds, index =", idx, "last_log_index=", LastLogIndex());
    throw std::out_of_range("Raft log index out of bounds");
  }

  auto it = m_log_indices.upper_bound(idx);
  Logger::Debug(it == m_log_indices.end(), m_log_indices.size());
  it--;
  auto& page = it->second;
  Logger::Debug("index =", page->start_index);
  return page->log_entries[idx - page->start_index];
}

std::vector<protocol::log::LogEntry> PersistedLog::Entries(std::size_t start, std::size_t end) const {
  if (start > end || end > LastLogIndex() + 1) {
    Logger::Error("Invalid iterator for raft log, start =", start, "end =", end, "last_log_index =", LastLogIndex());
    throw std::invalid_argument("Invalid iterators for raft log");
  }

  std::vector<protocol::log::LogEntry> queried_entries;
  std::size_t curr_index = start;
  while (curr_index < end) {
    auto it = m_log_indices.upper_bound(curr_index);
    auto& end_index = it == m_log_indices.end() ? m_open_page.end_index : it->first;
    it--;
    auto& page = it->second;

    if (end_index < end) {
      queried_entries.insert(
          queried_entries.end(),
          page->log_entries.begin() + curr_index - m_open_page.start_index,
          page->log_entries.end());
    } else {
      queried_entries.insert(
          queried_entries.end(),
          page->log_entries.begin() + curr_index - m_open_page.start_index,
          page->log_entries.begin() + end - page->start_index);
    }

    curr_index = end_index;
  }

  return queried_entries;
}

std::size_t PersistedLog::LogSize() const {
  return m_log_size;
}

std::tuple<protocol::log::LogMetadata, bool> PersistedLog::RestoreState() {
  bool valid_metadata = false;
  for (const auto& file:ListDirectoryContents(m_dir)) {
    if (file == "metadata") {
      std::string metadata_path = m_dir + file;
      if (!Empty(metadata_path)) {
        m_metadata = LoadMetadata(metadata_path);
        valid_metadata = true;
      }
      continue;
    }

    std::string log_path = m_dir + file;
    m_open_page = LoadLogEntries(log_path);
    m_log_size = m_open_page.log_entries.size();
    m_log_indices.insert({m_open_page.start_index, &m_open_page});
  }

  return std::make_tuple(m_metadata, valid_metadata);
}

void PersistedLog::PersistMetadata(const std::string& metadata_path) {
  std::fstream out(metadata_path, std::ios::out | std::ios::trunc | std::ios::binary);

  bool success = google::protobuf::util::SerializeDelimitedToOstream(m_metadata, &out);
  if (!success) {
    Logger::Error("Unexpected serialization failure when persisting raft metadata to disk");
    throw std::runtime_error("Unable to serialize raft metadata");
  }
  Logger::Debug("Persisted raft metadata successfully, term =", m_metadata.term(), "vote =", m_metadata.vote());
  out.flush();
}

void PersistedLog::PersistLogEntries(const std::vector<protocol::log::LogEntry>& new_entries) {
  std::string file_path = m_dir + m_open_page.filename;
  Logger::Debug("File path was", file_path);
  std::fstream out(file_path, std::ios::out | std::ios::app | std::ios::binary);

  bool success = true;
  for (auto &entry:new_entries) {
    if (entry.ByteSizeLong() > m_open_page.RemainingSpace()) {
      out.flush();
      CreatePage();
    }

    success = m_open_page.WriteLogEntry(out, entry);
    if (!success) {
      Logger::Error("Unexpected serialization failure when persisting raft log to disk");
      throw std::runtime_error("Unable to serialize raft log");
    }

    m_log_size++;
  }
  Logger::Debug("Persisted raft log entries successfully");
  out.flush();
}

protocol::log::LogMetadata PersistedLog::LoadMetadata(const std::string& metadata_path) const {
  std::fstream in(metadata_path, std::ios::in | std::ios::binary);

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

PersistedLog::Page PersistedLog::LoadLogEntries(const std::string& log_path) const {
  std::fstream in(log_path, std::ios::in | std::ios::binary);
  google::protobuf::io::IstreamInputStream log_stream(&in);
  std::vector<protocol::log::LogEntry> log_entries;
  protocol::log::LogEntry temp_log_entry;
  
  while (google::protobuf::util::ParseDelimitedFromZeroCopyStream(&temp_log_entry, &log_stream, nullptr)) {
    log_entries.push_back(temp_log_entry);
    Logger::Debug("READING:", temp_log_entry.term(), temp_log_entry.data());
  }

  Logger::Debug("Restored log entries from disk, log_path =", log_path, "size =", log_entries.size());

  auto loaded_page = Page(LogSize(), m_dir, m_max_file_size);
  loaded_page.log_entries = std::move(log_entries);
  return loaded_page;
}

}

