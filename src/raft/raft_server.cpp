#include "raft_server.h"
#include "global_ctx_manager.h"

namespace raft {

RaftServer::RaftServer(GlobalCtxManager& ctx)
  : m_ctx(ctx) {
  ServerInit(m_ctx.address);
}

RaftServer::~RaftServer() {
  if (m_server) {
    m_server->Shutdown();
  }
}

void RaftServer::ServerInit(const std::string& address) {
  grpc::EnableDefaultHealthCheckService(true);
  grpc::reflection::InitProtoReflectionServerBuilderPlugin();
  grpc::ServerBuilder builder;

  builder.AddListeningPort(address, grpc::InsecureServerCredentials());
  builder.RegisterService(&m_service);
  m_server = std::move(builder.BuildAndStart());
  Logger::Info("Server listening on", address);

  m_server->Wait();
}

void RaftServer::RequestVote(
    grpc::CallbackServerContext* request_ctx,
    const protocol::raft::RequestVote_Request* request,
    protocol::raft::RequestVote_Response* reply) {
  Logger::Debug("Processing RequestVote response");
}

void RaftServer::AppendEntries(
      grpc::CallbackServerContext* request_ctx,
      const protocol::raft::AppendEntries_Request* request,
      protocol::raft::AppendEntries_Response* reply) {
  Logger::Debug("Processing AppendEntries response");
}

}

