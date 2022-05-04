#ifndef GLOBAL_CTX_MANAGER_H
#define GLOBAL_CTX_MANAGER_H

#include <memory>
#include <string>
#include <vector>

#include "timer.h"

namespace raft {

class ConcensusModule;
class RaftClient;
class RaftServer;
class Snapshot;

class GlobalCtxManager : std::enable_shared_from_this<GlobalCtxManager> {
public:
  GlobalCtxManager(
      const std::string& address,
      const std::vector<std::string>& peer_ids);

public:
  std::string address;
  std::vector<std::string> peer_ids;
  ConcensusModule& concensus;
  RaftClient& client;
  RaftServer& server;
  Snapshot& log;
  std::shared_ptr<AsyncExecutor> executor;
  std::shared_ptr<timer::TimerQueue> timer_queue;
};

}

#endif

