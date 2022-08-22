#include "global_ctx_manager.h"
#include "cluster_configuration.h"
#include "consensus_module.h"
#include "raft_client.h"
#include "raft_server.h"
#include "storage.h"

namespace raft {

GlobalCtxManager::GlobalCtxManager(const std::string& address)
  : address(address)
  , m_consensus(std::make_shared<ConsensusModule>(*this))
  , m_client(std::make_shared<RaftClientImpl>(*this))
  , m_server(std::make_shared<RaftServerImpl>(*this))
  , m_log(std::make_shared<PersistedLog>("/data/raft/", true))
  , m_timer_queue(std::make_shared<core::TimerQueue>()) {
}

std::shared_ptr<ConsensusModule> GlobalCtxManager::ConsensusInstance() const {
  return m_consensus;
}

std::shared_ptr<RaftClientImpl> GlobalCtxManager::ClientInstance() const {
  return m_client;
}

std::shared_ptr<RaftServerImpl> GlobalCtxManager::ServerInstance() const {
  return m_server;
}

std::shared_ptr<Log> GlobalCtxManager::LogInstance() const {
  return m_log;
}

std::shared_ptr<core::TimerQueue> GlobalCtxManager::TimerQueueInstance() const {
  return m_timer_queue;
}

}

