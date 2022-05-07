#ifndef GLOBAL_CTX_MANAGER_H
#define GLOBAL_CTX_MANAGER_H

#include <memory>
#include <string>
#include <vector>

#include "async_executor.h"
#include "timer.h"

namespace raft {

class ConcensusModule;
class RaftClient;
class RaftServer;
class Snapshot;

class GlobalCtxManager {
public:
  GlobalCtxManager(
      const std::string& address,
      const std::vector<std::string>& peer_ids,
      const std::size_t delay);

  std::shared_ptr<ConcensusModule> ConcensusInstance() const;
  std::shared_ptr<RaftClient> ClientInstance() const;
  std::shared_ptr<RaftServer> ServerInstance() const;
  std::shared_ptr<Snapshot> LogInstance() const;
  std::shared_ptr<core::AsyncExecutor> ExecutorInstance() const;
  std::shared_ptr<core::TimerQueue> TimerQueueInstance() const;

private:
  std::shared_ptr<ConcensusModule> m_concensus;
  std::shared_ptr<RaftClient> m_client;
  std::shared_ptr<RaftServer> m_server;
  std::shared_ptr<Snapshot> m_log;
  std::shared_ptr<core::AsyncExecutor> m_executor;
  std::shared_ptr<core::TimerQueue> m_timer_queue;

public:
  std::string address;
  std::vector<std::string> peer_ids;
};

}

#endif

