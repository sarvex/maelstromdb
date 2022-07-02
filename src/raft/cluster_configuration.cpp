#include "cluster_configuration.h"

namespace raft {

ClusterConfiguration::ClusterConfiguration()
  : m_id(-1), m_state(ConfigurationState::EMPTY) {
}

protocol::log::Configuration ClusterConfiguration::Configuration() const {
  return m_current_configuration;
}

protocol::log::Configuration ClusterConfiguration::Configuration(int id) const {
  if (m_configurations.find(id) == m_configurations.end()) {
    Logger::Error("Cluster configuration at log index =", id, "does not exist");
    throw std::out_of_range("No configuration in log at index: " + std::to_string(id));
  }
  return m_configurations.at(id);
}

void ClusterConfiguration::SetConfiguration(int new_id, const protocol::log::Configuration& configuration) {
  Logger::Debug("Updating cluster configuration with id =", new_id);
  if (configuration.next_configuration().size() == 0) {
    SetState(ConfigurationState::STABLE);
    Logger::Debug("Cluster is now stable...");
  } else {
    SetState(ConfigurationState::JOINT);
  }
  m_id = new_id;
  m_current_configuration = configuration;
  m_addresses.clear();

  for (auto& server:configuration.prev_configuration()) {
    m_addresses.insert(server.address());
  }
  for (auto& server:configuration.next_configuration()) {
    m_addresses.insert(server.address());
  }
}

int ClusterConfiguration::Id() const {
  return m_id;
}

ClusterConfiguration::ConfigurationState ClusterConfiguration::State() const {
  return m_state;
}

void ClusterConfiguration::SetState(ClusterConfiguration::ConfigurationState new_state) {
  m_state = new_state;
}

void ClusterConfiguration::InsertNewConfiguration(int new_id, const protocol::log::Configuration& new_configuration) {
  m_configurations.insert({new_id, new_configuration});
  auto it = m_configurations.rbegin();
  if (Id() != it->first) {
    SetConfiguration(it->first, it->second);
  }
}

std::unordered_set<std::string> ClusterConfiguration::ServerAddresses() const {
  return m_addresses;
}

bool ClusterConfiguration::KnownServer(std::string address) {
  return m_addresses.find(address) != m_addresses.end();
}

void ClusterConfiguration::TruncateSuffix(int removal_index) {
  m_configurations.erase(m_configurations.upper_bound(removal_index), m_configurations.end());
  auto it = m_configurations.rbegin();
  if (Id() != it->first) {
    SetConfiguration(it->first, it->second);
  }
}

bool ClusterConfiguration::CheckQuorum(std::unordered_set<std::string> peer_votes) {
  if (State() == ConfigurationState::JOINT) {
    int prev_votes = 0;
    int next_votes = 0;
    for (auto old_server:m_current_configuration.prev_configuration()) {
      if (peer_votes.find(old_server.address()) != peer_votes.end()) {
        prev_votes++;
      }
    }
    for (auto new_server:m_current_configuration.next_configuration()) {
      if (peer_votes.find(new_server.address()) != peer_votes.end()) {
        next_votes++;
      }
    }
    bool prev_majority = prev_votes*2 > m_current_configuration.prev_configuration().size();
    bool next_majority = next_votes*2 > m_current_configuration.next_configuration().size();
    return prev_majority && next_majority;
  } else {
    return peer_votes.size()*2 > m_current_configuration.prev_configuration().size();
  }
}

}

