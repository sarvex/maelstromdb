#ifndef LEADER_PROXY_H
#define LEADER_PROXY_H

#include <functional>
#include <grpcpp/grpcpp.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "logger.h"
#include "raft.grpc.pb.h"

namespace raft {

class LeaderProxy {
public:
  using stub_map = std::unordered_map<std::string, std::unique_ptr<protocol::raft::RaftService::Stub>>;

public:
  LeaderProxy(const std::vector<std::string>& peers);

  void CreateConnections(std::vector<std::string> peer_addresses);

  protocol::raft::GetConfiguration_Response GetClusterConfiguration();
  protocol::raft::SetConfiguration_Response SetClusterConfiguration(
      int cluster_id,
      const std::vector<protocol::log::Server>& new_servers);

  protocol::raft::RegisterClient_Response RegisterClient();

private:
  void RedirectToLeader(
      std::function<grpc::Status(std::string)> func);

    grpc::Status Retry(
        std::string address,
        std::function<grpc::Status(std::string)> func);

  grpc::Status GetConfigurationRPC(
      std::string peer_id,
      protocol::raft::GetConfiguration_Response& reply);
  grpc::Status SetConfigurationRPC(
      std::string peer_id,
      int cluster_id,
      const std::vector<protocol::log::Server>& new_servers,
      protocol::raft::SetConfiguration_Response& reply);

  grpc::Status RegisterClientRPC(
      std::string peer_id,
      protocol::raft::RegisterClient_Response& reply);

private:
  std::string m_leader_hint;
  stub_map m_stubs;
};

}

#endif

