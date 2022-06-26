#ifndef CLUSTER_CONFIGURATION_H
#define CLUSTER_CONFIGURATION_H

#include <map>
#include <string>
#include <unordered_set>

#include "logger.h"

#include "raft.grpc.pb.h"

namespace raft {

class ClusterConfiguration {
public:
  enum class ConfigurationState {
    EMPTY,
    SYNC,
    JOINT,
    STABLE
  };

public:
  ClusterConfiguration();

  protocol::log::Configuration Configuration() const;
  protocol::log::Configuration Configuration(int id) const;
  void SetConfiguration(int new_id, const protocol::log::Configuration& configuration);

  int Id() const;

  ConfigurationState State() const;
  void SetState(ConfigurationState new_state);

  void InsertNewConfiguration(int new_id, const protocol::log::Configuration& new_configuration);

  std::unordered_set<std::string> ServerAddresses() const;

  bool KnownServer(std::string address);

  void TruncateSuffix(int removal_index);

  bool CheckQuorum(std::unordered_set<std::string> peer_votes);

private:
  int m_id;
  ConfigurationState m_state;
  protocol::log::Configuration m_current_configuration;
  std::unordered_set<std::string> m_addresses;
  std::map<int, protocol::log::Configuration> m_configurations;
};

}

#endif

