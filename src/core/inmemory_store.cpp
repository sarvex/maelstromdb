#include "inmemory_store.h"

InmemoryStore::InmemoryStore() {
}

std::string InmemoryStore::Read(const std::string& key) {
  return m_store.at(key);
}

void InmemoryStore::Write(std::string key, std::string value) {
  m_store[key] = value;
}

