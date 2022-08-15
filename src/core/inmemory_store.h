#ifndef INMEMORY_STORE_H
#define INMEMORY_STORE_H

#include <string>
#include <unordered_map>

class InmemoryStore {
public:
  InmemoryStore();

  std::string Read(const std::string& key);
  void Write(std::string key, std::string value);

private:
  std::unordered_map<std::string, std::string> m_store;
};

#endif

