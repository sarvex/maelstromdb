#include "session_cache.h"

namespace raft {

SessionCache::SessionCache(int size) : m_session_cache(size) {
}

void SessionCache::AddSession(int client_id) {
  Logger::Debug("Adding", client_id, "to session cache");
  m_session_cache.CreateNode(client_id);
}

void SessionCache::CacheResponse(
    int client_id, int sequence_num, protocol::raft::ClientRequest_Response& reply) {
  m_session_cache.UpdateNode(client_id, sequence_num, reply);
}

bool SessionCache::GetCachedResponse(
    int client_id, int sequence_num, protocol::raft::ClientRequest_Response& reply) {
  bool is_cached = true;
  try {
    reply = m_session_cache.NodeValue(client_id, sequence_num);
  } catch(const std::out_of_range& e) {
    is_cached = false;
  }
  return is_cached;
}

bool SessionCache::SessionExists(int client_id) {
  return m_session_cache.NodeExists(client_id);
}

SessionCache::ClientRequestLRUCache::ClientRequestLRUCache(int capacity)
  : m_capacity(capacity)
  , m_size(0) {
  m_head = std::make_shared<LRUNode>();
  m_tail = std::make_shared<LRUNode>();
  m_head->next = m_tail;
  m_tail->prev = m_head;
}

void SessionCache::ClientRequestLRUCache::CreateNode(int client_id) {
  std::lock_guard<std::mutex> guard(m_lock);
  if (m_cache.find(client_id) != m_cache.end()) {
    throw std::out_of_range("Session with id already exists");
  }
  m_cache[client_id] = std::make_shared<LRUNode>();
}

protocol::raft::ClientRequest_Response SessionCache::ClientRequestLRUCache::NodeValue(
    int client_id, int sequence_num) {
  std::lock_guard<std::mutex> guard(m_lock);
  return m_cache.at(client_id)->val.at(sequence_num);
}

void SessionCache::ClientRequestLRUCache::UpdateNode(
    int client_id, int sequence_num, protocol::raft::ClientRequest_Response& reply) {
  std::lock_guard<std::mutex> guard(m_lock);
  if (m_cache.find(client_id) == m_cache.end()) {
    auto new_node = std::make_shared<LRUNode>();
    new_node->val[sequence_num] = reply;
    m_cache[client_id] = std::move(new_node);
  } else {
    m_cache[client_id]->val[sequence_num] = reply;
  }
}

bool SessionCache::ClientRequestLRUCache::NodeExists(int client_id) {
  std::lock_guard<std::mutex> guard(m_lock);
  return m_cache.find(client_id) != m_cache.end();
}

SessionCache::ClientRequestLRUCache::LRUNode::LRUNode()
  : val({}), prev(nullptr), next(nullptr) {
}

}

