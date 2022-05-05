#include "global_ctx_manager.h"
#include "concensus_module.h"
#include "raft_client.h"
#include "raft_server.h"
#include "snapshot.h"

namespace raft {

GlobalCtxManager::GlobalCtxManager(
    const std::string& address,
    const std::vector<std::string>& peer_ids)
  : address(address)
  , peer_ids(peer_ids)
  , concensus(std::make_shared<ConcensusModule>(*this))
  , client(std::make_shared<RaftClient>(*this))
  , server(std::make_shared<RaftServer>(*this))
  , log(std::make_shared<Snapshot>(*this))
  , executor(std::make_shared<Strand>())
  , timer_queue(std::make_shared<timer::TimerQueue>()) {
  concensus->StateMachineInit();
  server->ServerInit();
}

}

