#include "raft_server.h"
#include "global_ctx_manager.h"

namespace raft {

AsyncServer::AsyncServer(GlobalCtxManager& ctx)
  : m_ctx(ctx) {
}

AsyncServer::~AsyncServer() {
}

AsyncServer::CallData::CallData(
    GlobalCtxManager& ctx,
    protocol::raft::RaftService::AsyncService* service,
    grpc::ServerCompletionQueue* scq)
  : m_ctx(ctx), m_service(service), m_scq(scq), m_status(CallStatus::CREATE) {
}

RaftServer::RaftServer(GlobalCtxManager& ctx)
  : AsyncServer(ctx) {
}

RaftServer::~RaftServer() {
  m_server->Shutdown();
  m_scq->Shutdown();
}

void RaftServer::ServerInit() {
  grpc::ServerBuilder builder;
  builder.AddListeningPort(m_ctx.address, grpc::InsecureServerCredentials());
  builder.RegisterService(&m_service);
  m_scq = builder.AddCompletionQueue();
  m_server = builder.BuildAndStart();
  Logger::Info("Server listening on", m_ctx.address);

  RpcEventLoop();
}

void RaftServer::RpcEventLoop() {
  new RaftServer::RequestVoteData(m_ctx, &m_service, m_scq.get());
  new RaftServer::AppendEntriesData(m_ctx, &m_service, m_scq.get());

  void* tag;
  bool ok;
  while (true) {
    if (m_scq->Next(&tag, &ok) && ok) {
      auto* tag_ptr = static_cast<RaftClient::Tag*>(tag);
      switch (tag_ptr->id) {
        case RaftClient::CommandID::REQUEST_VOTE: {
          static_cast<RaftServer::RequestVoteData*>(tag_ptr->call)->Proceed();
          break;
        }
        case RaftClient::CommandID::APPEND_ENTRIES: {
          static_cast<RaftServer::AppendEntriesData*>(tag_ptr->call)->Proceed();
          break;
        }
      }    
    } else {
      Logger::Info("RPC call failed unexpectedly");
    }
  }
}

RaftServer::RequestVoteData::RequestVoteData(
    GlobalCtxManager& ctx,
    protocol::raft::RaftService::AsyncService* service,
    grpc::ServerCompletionQueue* scq)
  : CallData(ctx, service, scq), m_responder(&m_server_ctx) {
  m_tag.id = RaftClient::CommandID::REQUEST_VOTE;
  m_tag.call = this;
  Proceed();
}

void RaftServer::RequestVoteData::Proceed() {
  switch (m_status) {
    case CallStatus::CREATE: {
      Logger::Debug("Creating RequestVote reply...");
      m_status = CallStatus::PROCESS;
      m_service->RequestRequestVote(
          &m_server_ctx,
          &m_request,
          &m_responder,
          m_scq,
          m_scq,
          (void*)&m_tag);
      break;
    }
    case CallStatus::PROCESS: {
      Logger::Debug("Processing RequestVote reply...");
      new RequestVoteData(m_ctx, m_service, m_scq);

      if (m_ctx.ConcensusInstance()->State() == ConcensusModule::RaftState::DEAD) {
        m_status = CallStatus::FINISH;
        m_responder.Finish(m_response, grpc::Status::CANCELLED, (void*)&m_tag);
        break;
      }

      if (m_request.term() > m_ctx.ConcensusInstance()->Term()) {
        Logger::Debug("Term out of date in RequestVote RPC, changed from", m_ctx.ConcensusInstance()->Term(), "to", m_request.term());
        m_ctx.ConcensusInstance()->ResetToFollower(m_request.term());
      }

      m_status = CallStatus::FINISH;
      m_responder.Finish(m_response, grpc::Status::OK, (void*)&m_tag);

      break;
    }
    case CallStatus::FINISH: {
      delete this;
    }
  }
}

RaftServer::AppendEntriesData::AppendEntriesData(
    GlobalCtxManager& ctx,
    protocol::raft::RaftService::AsyncService* service,
    grpc::ServerCompletionQueue* scq)
  : CallData(ctx, service, scq), m_responder(&m_server_ctx) {
  m_tag.id = RaftClient::CommandID::APPEND_ENTRIES;
  m_tag.call = this;
  Proceed();
}

void RaftServer::AppendEntriesData::Proceed() {
  switch (m_status) {
    case CallStatus::CREATE: {
      Logger::Debug("Creating AppendEntries reply...");
      m_status = CallStatus::PROCESS;
      m_service->RequestAppendEntries(
          &m_server_ctx,
          &m_request,
          &m_responder,
          m_scq,
          m_scq,
          (void*)&m_tag);
      break;
    }
    case CallStatus::PROCESS: {
      Logger::Debug("Processing AppendEntries reply...");
      new AppendEntriesData(m_ctx, m_service, m_scq);

      if (m_ctx.ConcensusInstance()->State() == ConcensusModule::RaftState::DEAD) {
        m_status = CallStatus::FINISH;
        m_responder.Finish(m_response, grpc::Status::CANCELLED, (void*)&m_tag);
        break;
      }

      if (m_request.term() > m_ctx.ConcensusInstance()->Term()) {
        Logger::Debug("Term out of date in AppendEntries RPC, changed from", m_ctx.ConcensusInstance()->Term(), "to", m_request.term());
        m_ctx.ConcensusInstance()->ResetToFollower(m_request.term());
      }

      m_status = CallStatus::FINISH;
      m_responder.Finish(m_response, grpc::Status::OK, (void*)&m_tag);

      break;
    }
    case CallStatus::FINISH: {
      delete this;
    }
  }
}

}

