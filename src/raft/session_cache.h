#ifndef SESSION_CACHE_H
#define SESSION_CACHE_H

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "logger.h"
#include "raft.grpc.pb.h"

namespace raft {

class SessionCache {
public:
  SessionCache(int size);

  void AddSession(int client_id);

  void SearchSession(int client_id, protocol::raft::RegisterClient_Response& reply);

private:
  template <typename T>
  class LRUCache {
  public:
    LRUCache(int capacity);

    T Get(int client_id, protocol::raft::RegisterClient_Response& reply);
    void Set(int client_id, T& reply);

  private:
    struct LRUNode {
      LRUNode(T& val);

      T val;
      std::shared_ptr<LRUNode> prev;
      std::shared_ptr<LRUNode> next;
    };

  private:
    int m_capacity;
    int m_size;
    std::mutex m_lock;
    std::unordered_map<int, std::shared_ptr<LRUNode>> m_cache;
    std::shared_ptr<LRUNode> m_head;
    std::shared_ptr<LRUNode> m_tail;
  };

private:
  LRUCache<protocol::raft::RegisterClient_Response> m_session_cache;
};

template <typename T>
SessionCache::LRUCache<T>::LRUCache(int capacity)
  : m_capacity(capacity)
  , m_size(0) {
  // m_head = new LRUNode(-1);
  // m_tail = new LRUNode(-1);
  // m_head->next = m_tail;
  // m_tail->prev = m_head;
}

template <typename T>
T SessionCache::LRUCache<T>::Get(int client_id, protocol::raft::RegisterClient_Response& reply) {
  std::lock_guard<std::mutex> guard(m_lock);
  return m_cache.at(client_id)->val;
}

template <typename T>
void SessionCache::LRUCache<T>::Set(int client_id, T& reply) {
  std::lock_guard<std::mutex> guard(m_lock);
  m_cache[client_id] = std::make_shared<LRUNode>(reply);
}

template <typename T>
SessionCache::LRUCache<T>::LRUNode::LRUNode(T& val)
  : val(val), prev(nullptr), next(nullptr) {
}

}

#endif

