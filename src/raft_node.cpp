#include <string>
#include <vector>

#include "global_ctx_manager.h"
#include "logger.h"

int main(int argc, char* argv[]) {
  Logger::SetLevel(Logger::LogLevel::DEBUG);
  Logger::Info("Initializing...");

  std::string address = argv[1];
  std::vector<std::string> peer_ids{};
  for (int i = 2; i < argc; i++) {
    peer_ids.push_back(argv[i]);
  }

  raft::GlobalCtxManager ctx(address, peer_ids);

  Logger::Info("Node terminated");
  return 0;
}

