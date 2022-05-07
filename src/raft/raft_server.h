#ifndef RAFT_SERVER_H
#define RAFT_SERVER_H

#include <grpcpp/grpcpp.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/health_check_service_interface.h>
#include <memory>

#include "concensus_module.h"
#include "logger.h"
#include "raft_client.h"
#include "raft.grpc.pb.h"

namespace raft {

class GlobalCtxManager;

class AsyncServer {
public:
  AsyncServer(GlobalCtxManager& ctx);
  virtual ~AsyncServer();

  virtual void ServerInit() = 0;

  virtual void RpcEventLoop() = 0;

protected:
  class CallData {
  public:
      CallData(
          GlobalCtxManager& ctx,
          protocol::raft::RaftService::AsyncService* service,
          grpc::ServerCompletionQueue* scq);

      virtual void Proceed() = 0;

  protected:
      enum class CallStatus {
          CREATE,
          PROCESS,
          FINISH
      };
      GlobalCtxManager& m_ctx;
      protocol::raft::RaftService::AsyncService* m_service;
      grpc::ServerCompletionQueue* m_scq;
      grpc::ServerContext m_server_ctx;
      CallStatus m_status;
  };

protected:
  GlobalCtxManager& m_ctx;
  std::unique_ptr<grpc::Server> m_server;
  std::unique_ptr<grpc::ServerCompletionQueue> m_scq;
};

class RaftServer : public AsyncServer {
public:
  RaftServer(GlobalCtxManager& ctx);
  ~RaftServer();

  void ServerInit() override;

  void RpcEventLoop() override;

  class RequestVoteData : public CallData {
  public:
      RequestVoteData(
          GlobalCtxManager& ctx,
          protocol::raft::RaftService::AsyncService* service,
          grpc::ServerCompletionQueue* scq);

      void Proceed() override;

  private:
      protocol::raft::RequestVote_Request m_request;
      protocol::raft::RequestVote_Response m_response;
      grpc::ServerAsyncResponseWriter<protocol::raft::RequestVote_Response> m_responder;
      RaftClient::Tag m_tag;
  };

  class AppendEntriesData : public CallData {
  public:
      AppendEntriesData(
          GlobalCtxManager& ctx,
          protocol::raft::RaftService::AsyncService* service,
          grpc::ServerCompletionQueue* scq);

      void Proceed() override;

  private:
      protocol::raft::AppendEntries_Request m_request;
      protocol::raft::AppendEntries_Response m_response;
      grpc::ServerAsyncResponseWriter<protocol::raft::AppendEntries_Response> m_responder;
      RaftClient::Tag m_tag;
  };

private:
  protocol::raft::RaftService::AsyncService m_service;
};

}

#endif

