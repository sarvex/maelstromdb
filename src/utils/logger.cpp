#include "logger.h"

Logger::Logger() {
}

Logger::~Logger() {
}

void Logger::SetLevel(LogLevel new_level) {
  GetInstance().m_level = new_level;
}

void Logger::SetLogFile(std::string address) {
  GetInstance().m_type = Logger::LogType::FILE;
  std::string abs_file_dir = "logs/" + address + "/";
  std::string abs_file_path = abs_file_dir + "log.txt";
  std::filesystem::create_directories(abs_file_dir);
  GetInstance().m_log_file.open(abs_file_path);
}

void Logger::SetLogConsole() {
  GetInstance().m_type = Logger::LogType::CONSOLE;
}

Logger& Logger::GetInstance() {
  static Logger singleton{};
  return singleton;
}

std::string Logger::Timestamp() {
  std::chrono::system_clock::time_point tp = std::chrono::system_clock::now();
  std::time_t tt = std::chrono::system_clock::to_time_t(tp);
  std::tm gmt{};
  gmtime_r(&tt, &gmt);
  std::chrono::duration<double> fractional_seconds =
    (tp - std::chrono::system_clock::from_time_t(tt)) + std::chrono::seconds(gmt.tm_sec);

  std::string buffer("year/mo/dy hr:mn:sc.xxxxxx");
  sprintf(&buffer.front(), "%04d/%02d/%02d %02d:%02d:%09.6f", gmt.tm_year + 1900, gmt.tm_mon + 1,
      gmt.tm_mday, gmt.tm_hour, gmt.tm_min, fractional_seconds.count());
  return buffer;
}

