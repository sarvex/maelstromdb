#include <string>
#include <vector>

#include "concensus_module.h"
#include "global_ctx_manager.h"
#include "logger.h"
#include "raft_client.h"
#include "raft_server.h"

int main(int argc, char* argv[]) {
  Logger::SetLevel(Logger::LogLevel::DEBUG);
  Logger::Info("Initializing...");

  std::string address(argv[1]);
  std::vector<std::string> peer_ids{};
  for (int i = 2; i < argc; i++) {
    peer_ids.push_back(argv[i]);
  }

  raft::GlobalCtxManager ctx(address, peer_ids);
  ctx.ConcensusInstance()->StateMachineInit(2);

  ctx.ClientInstance()->ClientInit();
  std::thread client_worker = std::thread(&raft::RaftClient::AsyncCompleteRPC, ctx.ClientInstance());

  ctx.ServerInstance()->ServerInit();

  if (client_worker.joinable()) {
    client_worker.join();
  }

  Logger::Info("Node terminated");
  return 0;
}

