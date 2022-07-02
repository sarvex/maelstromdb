#ifndef GLOBAL_CTX_MANAGER_H
#define GLOBAL_CTX_MANAGER_H

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "timer.h"

namespace raft {

class ClusterConfiguration;
class ConcensusModule;
class Log;
class RaftClientImpl;
class RaftServerImpl;

class GlobalCtxManager {
public:
  GlobalCtxManager(const std::string& address);

  std::shared_ptr<ConcensusModule> ConcensusInstance() const;
  std::shared_ptr<RaftClientImpl> ClientInstance() const;
  std::shared_ptr<RaftServerImpl> ServerInstance() const;
  std::shared_ptr<Log> LogInstance() const;
  std::shared_ptr<core::TimerQueue> TimerQueueInstance() const;

private:
  std::shared_ptr<ConcensusModule> m_concensus;
  std::shared_ptr<RaftClientImpl> m_client;
  std::shared_ptr<RaftServerImpl> m_server;
  std::shared_ptr<Log> m_log;
  std::shared_ptr<core::TimerQueue> m_timer_queue;

public:
  std::string address;
};

}

#endif

