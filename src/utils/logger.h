#ifndef LOGGER_H
#define LOGGER_H

#include <iostream>
#include <mutex>
#include <ctime>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <chrono>

class Logger {
public:
  enum class LogLevel {
    DEBUG,
    INFO,
    WARN,
    ERROR
  };

  struct EnumHasher { template <typename T> std::size_t operator()(T t) const { return static_cast<std::size_t>(t); } };
  const std::unordered_map<LogLevel, std::string, EnumHasher> colored
  {
    {LogLevel::ERROR, " \x1b[31;1m[ERROR]\x1b[0m "}, {LogLevel::WARN, " \x1b[33;1m[WARN]\x1b[0m "},
      {LogLevel::INFO, " \x1b[32;1m[INFO]\x1b[0m "}, {LogLevel::DEBUG, " \x1b[34;1m[DEBUG]\x1b[0m "}
  };

  static void SetLevel(LogLevel new_level);
  
  template <typename... Args>
  static void Debug(Args... args); 

  template <typename... Args>
  static void Info(Args... args); 

  template <typename... Args>
  static void Warn(Args... args); 

  template <typename... Args>
  static void Error(Args... args); 

private:
  LogLevel m_level = LogLevel::INFO;
  std::mutex m_Log_mutex;

private:
  Logger();
  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;
  ~Logger();

  static Logger& GetInstance();

  std::string Timestamp();

  template <typename... Args>
  void Log(LogLevel level, Args ...args);
  template <typename... Args>
  void LogMessage(Args&&... args);
};

template <typename... Args>
void Logger::Debug(Args... args) {
  GetInstance().Log(LogLevel::DEBUG, args...);
}

template <typename... Args>
void Logger::Info(Args... args) {
  GetInstance().Log(LogLevel::INFO, args...);
}

template <typename... Args>
void Logger::Warn(Args... args) {
  GetInstance().Log(LogLevel::WARN, args...);
}

template <typename... Args>
void Logger::Error(Args... args) {
  GetInstance().Log(LogLevel::ERROR, args...);
}

template <typename... Args>
void Logger::Log(LogLevel level, Args... args) {
  if (level >= m_level) {
    std::lock_guard<std::mutex> lock(m_Log_mutex);
    auto now = Timestamp();
    auto color = colored.find(level);
    if (color == colored.end()) {
      std::runtime_error("Could not produce logger");
    }
    auto level_str = color->second;
    LogMessage(now, level_str, args...);
  }
}

template <typename... Args>
void Logger::LogMessage(Args&&... args) {
  ((std::cout << " " << args), ...) << '\n';
}

#endif

