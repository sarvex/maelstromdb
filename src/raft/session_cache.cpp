#include "session_cache.h"

namespace raft {

SessionCache::SessionCache(int size) : m_session_cache(size) {
}

void SessionCache::AddSession(int client_id) {
  Logger::Debug("Adding", client_id, "to session cache");
  protocol::raft::RegisterClient_Response reply;
  m_session_cache.Set(client_id, reply);
}

void SessionCache::SearchSession(int client_id, protocol::raft::RegisterClient_Response& reply) {
  m_session_cache.Get(client_id, reply);
}

}

