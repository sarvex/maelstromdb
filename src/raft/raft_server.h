#ifndef RAFT_SERVER_H
#define RAFT_SERVER_H

#include <grpcpp/grpcpp.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/health_check_service_interface.h>
#include <memory.h>

#include "logger.h"
#include "raft.grpc.pb.h"

namespace raft {

class GlobalCtxManager;

class RaftServer {
public:
  RaftServer(GlobalCtxManager& ctx);
  ~RaftServer();

  void ServerInit(const std::string& address);

  void RequestVote(
      grpc::CallbackServerContext* request_ctx,
      const protocol::raft::RequestVote_Request* request,
      protocol::raft::RequestVote_Response* reply);

  void AppendEntries(
      grpc::CallbackServerContext* request_ctx,
      const protocol::raft::AppendEntries_Request* request,
      protocol::raft::AppendEntries_Response* reply);

private:
  GlobalCtxManager& m_ctx;
  protocol::raft::RaftService::AsyncService m_service;
  std::unique_ptr<grpc::Server> m_server;
};

}

#endif

