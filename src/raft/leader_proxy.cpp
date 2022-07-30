#include "leader_proxy.h"

namespace raft {

LeaderProxy::LeaderProxy(const std::vector<std::string>& peers) {
  CreateConnections(peers);
}

void LeaderProxy::CreateConnections(std::vector<std::string> peer_addresses) {
  for (auto& address:peer_addresses) {
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

protocol::raft::RegisterClient_Response LeaderProxy::RegisterClient() {
  protocol::raft::RegisterClient_Response reply;
  auto call = std::bind(&LeaderProxy::RegisterClientRPC, this, std::placeholders::_1, std::ref(reply));
  RedirectToLeader(call);
  return reply;
}

protocol::raft::ClientRequest_Response LeaderProxy::ClientRequest(
    int client_id,
    int sequence_num,
    std::string command) {
  protocol::raft::ClientRequest_Response reply;
  auto call = std::bind(&LeaderProxy::ClientRequestRPC, this, std::placeholders::_1,
                        client_id, sequence_num, command, std::ref(reply));
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

grpc::Status LeaderProxy::RegisterClientRPC(
    std::string peer_id,
    protocol::raft::RegisterClient_Response& reply) {
  grpc::ClientContext ctx;
  protocol::raft::RegisterClient_Request request_args;

  grpc::Status status = m_stubs[peer_id]->RegisterClient(&ctx, request_args, &reply);
  return status;
}

grpc::Status LeaderProxy::ClientRequestRPC(
    std::string peer_id,
    int client_id,
    int sequence_num,
    std::string command,
    protocol::raft::ClientRequest_Response& reply) {
  grpc::ClientContext ctx;
  protocol::raft::ClientRequest_Request request_args;
  request_args.set_clientid(client_id);
  request_args.set_sequencenum(sequence_num);
  request_args.set_command(command);

  grpc::Status status = m_stubs[peer_id]->ClientRequest(&ctx, request_args, &reply);
  return status;
}

}

