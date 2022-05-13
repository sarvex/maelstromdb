#ifndef GLOBAL_CTX_MANAGER_H
#define GLOBAL_CTX_MANAGER_H

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "timer.h"

namespace raft {

class ConcensusModule;
class RaftClient;
class RaftServer;
class Log;

class GlobalCtxManager {
public:
  GlobalCtxManager(
      const std::string& address,
      const std::vector<std::string>& peer_ids);

  std::shared_ptr<ConcensusModule> ConcensusInstance() const;
  std::shared_ptr<RaftClient> ClientInstance() const;
  std::shared_ptr<RaftServer> ServerInstance() const;
  std::shared_ptr<Log> LogInstance() const;
  std::shared_ptr<core::TimerQueue> TimerQueueInstance() const;

private:
  std::shared_ptr<ConcensusModule> m_concensus;
  std::shared_ptr<RaftClient> m_client;
  std::shared_ptr<RaftServer> m_server;
  std::shared_ptr<Log> m_log;
  std::shared_ptr<core::TimerQueue> m_timer_queue;

public:
  std::string address;
  std::vector<std::string> peer_ids;
};

}

#endif

