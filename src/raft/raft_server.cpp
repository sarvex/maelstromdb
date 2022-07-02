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

  RPCEventLoop();
}

void RaftServer::RPCEventLoop() {
  new RaftServer::RequestVoteData(m_ctx, &m_service, m_scq.get());
  new RaftServer::AppendEntriesData(m_ctx, &m_service, m_scq.get());
  new RaftServer::GetConfigurationData(m_ctx, &m_service, m_scq.get());
  new RaftServer::SetConfigurationData(m_ctx, &m_service, m_scq.get());

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
        case RaftClient::CommandID::GET_CONFIGURATION: {
          static_cast<RaftServer::GetConfigurationData*>(tag_ptr->call)->Proceed();
          break;
        }
        case RaftClient::CommandID::SET_CONFIGURATION: {
          static_cast<RaftServer::SetConfigurationData*>(tag_ptr->call)->Proceed();
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

      auto [m_response, s] = m_ctx.ConcensusInstance()->ProcessRequestVoteClientRequest(m_request);

      m_status = CallStatus::FINISH;
      m_responder.Finish(m_response, s, (void*)&m_tag);
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

      auto [m_response, s] = m_ctx.ConcensusInstance()->ProcessAppendEntriesClientRequest(m_request);

      m_status = CallStatus::FINISH;
      m_responder.Finish(m_response, s, (void*)&m_tag);
      break;
    }
    case CallStatus::FINISH: {
      delete this;
    }
  }
}

RaftServer::SetConfigurationData::SetConfigurationData(
    GlobalCtxManager& ctx,
    protocol::raft::RaftService::AsyncService* service,
    grpc::ServerCompletionQueue* scq)
  : CallData(ctx, service, scq), m_responder(&m_server_ctx) {
  m_tag.id = RaftClient::CommandID::SET_CONFIGURATION;
  m_tag.call = this;
  Proceed();
}

void RaftServer::SetConfigurationData::Proceed() {
  switch (m_status) {
    case CallStatus::CREATE: {
      m_status = CallStatus::PROCESS;
      m_service->RequestSetConfiguration(
          &m_server_ctx,
          &m_request,
          &m_responder,
          m_scq,
          m_scq,
          (void*)&m_tag);
      break;
    }
    case CallStatus::PROCESS: {
      Logger::Debug("Processing SetConfiguration reply...");
      new SetConfigurationData(m_ctx, m_service, m_scq);

      auto [m_response, s] = m_ctx.ConcensusInstance()->ProcessSetConfigurationClientRequest(m_request);

      m_status = CallStatus::FINISH;
      m_responder.Finish(m_response, s, (void*)&m_tag);
      break;
    }
    case CallStatus::FINISH: {
      delete this;
    }
  }
}

RaftServer::GetConfigurationData::GetConfigurationData(
    GlobalCtxManager& ctx,
    protocol::raft::RaftService::AsyncService* service,
    grpc::ServerCompletionQueue* scq)
  : CallData(ctx, service, scq), m_responder(&m_server_ctx) {
  m_tag.id = RaftClient::CommandID::GET_CONFIGURATION;
  m_tag.call = this;
  Proceed();
}

void RaftServer::GetConfigurationData::Proceed() {
  switch (m_status) {
    case CallStatus::CREATE: {
      m_status = CallStatus::PROCESS;
      m_service->RequestGetConfiguration(
          &m_server_ctx,
          &m_request,
          &m_responder,
          m_scq,
          m_scq,
          (void*)&m_tag);
      break;
    }
    case CallStatus::PROCESS: {
      Logger::Debug("Processing GetConfiguration reply...");
      new GetConfigurationData(m_ctx, m_service, m_scq);

      auto [m_response, s] = m_ctx.ConcensusInstance()->ProcessGetConfigurationClientRequest();

      m_status = CallStatus::FINISH;
      m_responder.Finish(m_response, s, (void*)&m_tag);
      break;
    }
    case CallStatus::FINISH: {
      delete this;
    }
  }
}

}

