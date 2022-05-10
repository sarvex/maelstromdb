#include "raft_client.h"
#include "global_ctx_manager.h"

namespace raft {

RaftClient::RaftClient(GlobalCtxManager& ctx)
  : m_ctx(ctx) {
}

void RaftClient::ClientInit() {
  for (auto& peer:m_ctx.peer_ids) {
    std::shared_ptr<grpc::Channel> chan = grpc::CreateChannel(peer, grpc::InsecureChannelCredentials());
    m_stubs[peer] = protocol::raft::RaftService::NewStub(chan);
  }
}

void RaftClient::RequestVote(
    const std::string& peer_id,
    const std::size_t term,
    const std::size_t last_log_index,
    const std::size_t last_log_term) {
  protocol::raft::RequestVote_Request request_args;
  request_args.set_term(term);
  request_args.set_candidateid(m_ctx.address);
  request_args.set_lastlogindex(last_log_index);
  request_args.set_lastlogterm(last_log_term);

  auto* call = new AsyncClientCall<protocol::raft::RequestVote_Request, protocol::raft::RequestVote_Response>;

  call->request = request_args;
  call->response_reader = m_stubs[peer_id]->PrepareAsyncRequestVote(&call->ctx, request_args, &m_cq);
  call->response_reader->StartCall();

  auto* tag = new Tag;
  tag->call = (void*)call;
  tag->id = CommandID::REQUEST_VOTE;

  call->response_reader->Finish(&call->reply, &call->status, (void*)tag);
}

void RaftClient::AppendEntries(
    const std::string& peer_id,
    const std::size_t term,
    const std::size_t prev_log_index,
    const std::size_t prev_log_term,
    const std::size_t leader_commit) {
  protocol::raft::AppendEntries_Request request_args;
  request_args.set_term(term);
  request_args.set_leaderid(m_ctx.address);
  request_args.set_prevlogindex(prev_log_index);
  request_args.set_prevlogterm(prev_log_term);
  request_args.set_leadercommit(leader_commit);

  auto* call = new AsyncClientCall<protocol::raft::AppendEntries_Request, protocol::raft::AppendEntries_Response>;

  call->request = request_args;
  call->response_reader = m_stubs[peer_id]->PrepareAsyncAppendEntries(&call->ctx, request_args, &m_cq);
  call->response_reader->StartCall();

  auto* tag = new Tag;
  tag->call = (void*)call;
  tag->id = CommandID::APPEND_ENTRIES;

  call->response_reader->Finish(&call->reply, &call->status, (void*)tag);
}

void RaftClient::AsyncCompleteRPC() {
  void* tag;
  bool ok = false;

  while (m_cq.Next(&tag, &ok)) {
    GPR_ASSERT(ok);

    auto* tag_ptr = static_cast<Tag*>(tag);
    switch (tag_ptr->id) {
      case CommandID::REQUEST_VOTE: {
        auto* call = static_cast<AsyncClientCall<protocol::raft::RequestVote_Request,
          protocol::raft::RequestVote_Response>*>(tag_ptr->call);

        HandleRequestVoteReply(call);

        delete call;
        break;
      }
      case CommandID::APPEND_ENTRIES: {
        auto* call = static_cast<AsyncClientCall<protocol::raft::AppendEntries_Request,
          protocol::raft::AppendEntries_Response>*>(tag_ptr->call);

        HandleAppendEntriesReply(call);

        delete call;
        break;
      }
    }

    delete tag_ptr;
  }
}

void RaftClient::HandleRequestVoteReply(AsyncClientCall<protocol::raft::RequestVote_Request,
      protocol::raft::RequestVote_Response>* call) {
  if (!call->status.ok()) {
    Logger::Info("RequestVote call failed unexpectedly"); 
    return;
  }

  m_ctx.ConcensusInstance()->ProcessRequestVoteServerResponse(call->request, call->reply);

  Logger::Debug("RequestVote call was received");
}

void RaftClient::HandleAppendEntriesReply(AsyncClientCall<protocol::raft::AppendEntries_Request,
      protocol::raft::AppendEntries_Response>* call) {
  if (!call->status.ok()) {
    Logger::Info("AppendEntries call failed unexpectedly");
    return;
  }

  m_ctx.ConcensusInstance()->ProcessAppendEntriesServerResponse(call->request, call->reply, call->ctx.peer().substr(5));

  Logger::Debug("AppendEntries call was received");
}

}

