#include "leader_proxy.h"

namespace raft {

LeaderProxy::LeaderProxy(const std::vector<std::string>& peers) {
  std::unordered_set<std::string> addresses;
  for (auto& peer:peers) {
    addresses.insert(peer);
  }
  CreateConnections(addresses);
}

void LeaderProxy::CreateConnections(std::unordered_set<std::string> peer_addresses) {
  std::vector<std::string> new_addresses;
  std::vector<std::string> removed_addresses;
  for (auto& peer_id:peer_addresses) {
    if (m_stubs.find(peer_id) == m_stubs.end()) {
      new_addresses.push_back(peer_id);
    }
  }
  for (auto& connection:m_stubs) {
    if (peer_addresses.find(connection.first) == peer_addresses.end()) {
      removed_addresses.push_back(connection.first);
    }
  }

  for (auto& address:removed_addresses) {
    m_stubs.erase(address);
  }
  for (auto& address:new_addresses) {
    std::shared_ptr<grpc::Channel> chan = grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
    m_stubs[address] = protocol::raft::RaftService::NewStub(chan);
  }
}

protocol::raft::GetConfiguration_Response LeaderProxy::GetClusterConfiguration() {
  std::unordered_set<std::string> visited;
  protocol::raft::GetConfiguration_Response reply;
  for (auto& peer:m_stubs) {
    std::string address = peer.first;
    grpc::Status status = GetConfigurationRPC(address, reply);
    if (status.ok()) {
      break;
    }
    protocol::raft::Error err;
    err.ParseFromString(status.error_details());
    visited.insert(address);
  }
  return reply;
}

protocol::raft::SetConfiguration_Response LeaderProxy::SetClusterConfiguration(
    int cluster_id,
    const std::vector<protocol::log::Server>& new_servers) {
  std::unordered_set<std::string> visited;
  protocol::raft::SetConfiguration_Response reply;
  for (auto& peer:m_stubs) {
    std::string address = peer.first;
    grpc::Status status = SetConfigurationRPC(address, cluster_id, new_servers, reply);
    if (status.ok()) {
      break;
    }
    protocol::raft::Error err;
    err.ParseFromString(status.error_details());
    visited.insert(address);
  }
  return reply;
}

grpc::Status LeaderProxy::GetConfigurationRPC(
    std::string& peer_id,
    protocol::raft::GetConfiguration_Response& reply) {
  grpc::ClientContext ctx;
  protocol::raft::GetConfiguration_Request request_args;

  grpc::Status status = m_stubs[peer_id]->GetConfiguration(&ctx, request_args, &reply);

  return status;
}

grpc::Status LeaderProxy::SetConfigurationRPC(
    std::string& peer_id,
    int cluster_id,
    const std::vector<protocol::log::Server>& new_servers,
    protocol::raft::SetConfiguration_Response& reply) {
  grpc::ClientContext ctx;
  protocol::raft::SetConfiguration_Request request_args;
  request_args.set_oldid(cluster_id);
  for (auto& server:new_servers) {
    *request_args.add_new_servers() = server;
  }

  grpc::Status status = m_stubs[peer_id]->SetConfiguration(&ctx, request_args, &reply);

  return status;
}

}

