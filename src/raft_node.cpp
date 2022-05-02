#include "logger.h"

int main(int argc, char** argv) {
  Logger::SetLevel(Logger::LogLevel::DEBUG);

  Logger::Info("Initializing...");

  return 0;
}

