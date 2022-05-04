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
  , peer_ids(peer_ids) {
}

}

