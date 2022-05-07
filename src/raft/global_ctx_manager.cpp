#include "global_ctx_manager.h"
#include "concensus_module.h"
#include "raft_client.h"
#include "raft_server.h"
#include "snapshot.h"

namespace raft {

GlobalCtxManager::GlobalCtxManager(
    const std::string& address,
    const std::vector<std::string>& peer_ids,
    const std::size_t delay)
  : address(address)
  , peer_ids(peer_ids)
  , m_concensus(std::make_shared<ConcensusModule>(*this))
  , m_client(std::make_shared<RaftClient>(*this))
  , m_server(std::make_shared<RaftServer>(*this))
  , m_log(std::make_shared<Snapshot>(*this))
  , m_executor(std::make_shared<core::Strand>())
  , m_timer_queue(std::make_shared<core::TimerQueue>()) {
  m_concensus->StateMachineInit(delay);

  m_client->ClientInit();
  std::thread client_worker = std::thread(&RaftClient::AsyncCompleteRPC, m_client);

  m_server->ServerInit();

  if (client_worker.joinable()) {
    client_worker.join();
  }
}

std::shared_ptr<ConcensusModule> GlobalCtxManager::ConcensusInstance() const {
  return m_concensus;
}

std::shared_ptr<RaftClient> GlobalCtxManager::ClientInstance() const {
  return m_client;
}

std::shared_ptr<RaftServer> GlobalCtxManager::ServerInstance() const {
  return m_server;
}

std::shared_ptr<Snapshot> GlobalCtxManager::LogInstance() const {
  return m_log;
}

std::shared_ptr<core::AsyncExecutor> GlobalCtxManager::ExecutorInstance() const {
  return m_executor;
}

std::shared_ptr<core::TimerQueue> GlobalCtxManager::TimerQueueInstance() const {
  return m_timer_queue;
}

}

