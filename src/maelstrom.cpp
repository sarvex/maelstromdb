#include <string>
#include <vector>

#include "concensus_module.h"
#include "global_ctx_manager.h"
#include "logger.h"
#include "raft_client.h"
#include "raft_server.h"

int main(int argc, char* argv[]) {
  std::string address(argv[1]);
  std::vector<std::string> peer_ids{};
  for (int i = 2; i < argc; i++) {
    peer_ids.push_back(argv[i]);
  }

  Logger::SetLevel(Logger::LogLevel::DEBUG);
  Logger::SetLogConsole();

  Logger::Info("Initializing...");

  raft::GlobalCtxManager ctx(address);
  ctx.ConcensusInstance()->StateMachineInit(2);
  ctx.ConcensusInstance()->InitializeConfiguration(peer_ids);

  std::thread client_worker = std::thread(&raft::RaftClient::AsyncCompleteRPC, ctx.ClientInstance());

  ctx.ServerInstance()->ServerInit();

  if (client_worker.joinable()) {
    client_worker.join();
  }

  Logger::Info("Node terminated");
  return 0;
}

