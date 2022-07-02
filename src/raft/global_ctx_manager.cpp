#include "global_ctx_manager.h"
#include "cluster_configuration.h"
#include "concensus_module.h"
#include "raft_client.h"
#include "raft_server.h"
#include "storage.h"

namespace raft {

GlobalCtxManager::GlobalCtxManager(const std::string& address)
  : address(address)
  , m_concensus(std::make_shared<ConcensusModule>(*this))
  , m_client(std::make_shared<RaftClientImpl>(*this))
  , m_server(std::make_shared<RaftServerImpl>(*this))
  , m_log(std::make_shared<PersistedLog>(std::filesystem::current_path().string() + "/raft/" + address + "/"))
  , m_timer_queue(std::make_shared<core::TimerQueue>()) {
}

std::shared_ptr<ConcensusModule> GlobalCtxManager::ConcensusInstance() const {
  return m_concensus;
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

