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
  protocol::raft::GetConfiguration_Response reply;
  auto call = std::bind(&LeaderProxy::GetConfigurationRPC, this, std::placeholders::_1, std::ref(reply));
  RedirectToLeader(call);
  return reply;
}

protocol::raft::SetConfiguration_Response LeaderProxy::SetClusterConfiguration(
    int cluster_id,
    const std::vector<protocol::log::Server>& new_servers) {
  protocol::raft::SetConfiguration_Response reply;
  auto call = std::bind(&LeaderProxy::SetConfigurationRPC, this, std::placeholders::_1, cluster_id, new_servers, std::ref(reply));
  RedirectToLeader(call);
  return reply;
}

void LeaderProxy::RedirectToLeader(
      std::function<grpc::Status(std::string)> func) {
  std::unordered_set<std::string> visited;
  grpc::Status status;
  for (auto& peer:m_stubs) {
    std::string address = peer.first;
    if (visited.find(address) != visited.end()) {
      continue;
    }
    visited.insert(address);

    protocol::raft::Error err;
    status = Retry(address, func);
    if (status.ok()) {
      return;
    }
    err.ParseFromString(status.error_details());

    if (err.statuscode() == protocol::raft::Error::NOT_LEADER &&
        err.leaderhint().size() > 0 &&
        visited.find(err.leaderhint()) == visited.end()) {
      status = Retry(err.leaderhint(), func);
      visited.insert(err.leaderhint());
    }
  }
}

grpc::Status LeaderProxy::Retry(
    std::string address,
    std::function<grpc::Status(std::string)> func) {
  protocol::raft::Error err;
  grpc::Status status;
  err.set_statuscode(protocol::raft::Error::RETRY);
  while (err.statuscode() == protocol::raft::Error::RETRY) {
    status = func(address);
    if (status.ok()) {
      return status;
    }
    err.ParseFromString(status.error_details());
  }
  return status;
}

grpc::Status LeaderProxy::GetConfigurationRPC(
    std::string peer_id,
    protocol::raft::GetConfiguration_Response& reply) {
  grpc::ClientContext ctx;
  protocol::raft::GetConfiguration_Request request_args;

  grpc::Status status = m_stubs[peer_id]->GetConfiguration(&ctx, request_args, &reply);

  return status;
}

grpc::Status LeaderProxy::SetConfigurationRPC(
    std::string peer_id,
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

