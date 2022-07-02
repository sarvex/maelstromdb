#include "raft_client.h"
#include "global_ctx_manager.h"
#include "raft.pb.h"

namespace raft {

RaftClient::RaftClient(GlobalCtxManager& ctx)
  : m_ctx(ctx) {
}

void RaftClient::CreateConnections(std::unordered_set<std::string> peer_addresses) {
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

void RaftClient::RequestVote(
    const std::string& peer_id,
    const int term,
    const int last_log_index,
    const int last_log_term) {
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
    const int term,
    const int prev_log_index,
    const int prev_log_term,
    const std::vector<protocol::log::LogEntry> entries,
    const int leader_commit) {
  protocol::raft::AppendEntries_Request request_args;
  request_args.set_term(term);
  request_args.set_leaderid(m_ctx.address);
  request_args.set_prevlogindex(prev_log_index);
  request_args.set_prevlogterm(prev_log_term);
  request_args.set_leadercommit(leader_commit);

  for (auto& entry:entries) {
    *request_args.add_entries() = entry;
  }

  auto* call = new AsyncClientCall<protocol::raft::AppendEntries_Request, protocol::raft::AppendEntries_Response>;

  call->request = request_args;
  call->response_reader = m_stubs[peer_id]->PrepareAsyncAppendEntries(&call->ctx, request_args, &m_cq);
  call->response_reader->StartCall();

  auto* tag = new Tag;
  tag->call = (void*)call;
  tag->id = CommandID::APPEND_ENTRIES;

  call->response_reader->Finish(&call->reply, &call->status, (void*)tag);
}

// protocol::raft::GetConfiguration_Response RaftClient::GetClusterConfiguration(const std::string& peer_id) {
//   protocol::raft::GetConfiguration_Request request_args;
//   auto* call = new AsyncClientCall<protocol::raft::GetConfiguration_Request, protocol::raft::GetConfiguration_Response>;
//
//   call->request = request_args;
//   call->response_reader = m_stubs[peer_id]->PrepareAsyncGetConfiguration(&call->ctx, request_args, &m_cq);
//   call->response_reader->StartCall();
//
//   auto* tag = new Tag;
//   tag->call = (void*)call;
//   tag->id = CommandID::GET_CONFIGURATION;
//
//   call->response_reader->Finish(&call->reply, &call->status, (void*)tag);
// }

// protocol::raft::SetConfiguration_Response RaftClient::SetClusterConfiguration(
//     const std::string& peer_id,
//     const int old_id,
//     const std::vector<protocol::log::Server> new_servers) {
//   protocol::raft::SetConfiguration_Request request_args;
//   request_args.set_oldid(old_id);
//
//   for (auto& server:new_servers) {
//     *request_args.add_new_servers() = server;
//   }
//
//   auto* call = new AsyncClientCall<protocol::raft::SetConfiguration_Request, protocol::raft::SetConfiguration_Response>;
//
//   call->request = request_args;
//   call->response_reader = m_stubs[peer_id]->PrepareAsyncSetConfiguration(&call->ctx, request_args, &m_cq);
//   call->response_reader->StartCall();
//
//   auto* tag = new Tag;
//   tag->call = (void*)call;
//   tag->id = CommandID::SET_CONFIGURATION;
//
//   call->response_reader->Finish(&call->reply, &call->status, (void*)tag);
// }

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
      // case CommandID::SET_CONFIGURATION: {
      //   auto* call = static_cast<AsyncClientCall<protocol::raft::SetConfiguration_Request,
      //     protocol::raft::SetConfiguration_Response>*>(tag_ptr->call);
      //
      //   HandleSetConfigurationReply(call);
      //
      //   delete call;
      //   break;
      // }
      // case CommandID::GET_CONFIGURATION: {
      //   auto* call = static_cast<AsyncClientCall<protocol::raft::GetConfiguration_Request,
      //     protocol::raft::GetConfiguration_Response>*>(tag_ptr->call);
      //
      //   HandleGetConfigurationReply(call);
      //
      //   delete call;
      //   break;
      // }
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

  std::string peer_address = call->ctx.peer().substr(5);
  m_ctx.ConcensusInstance()->ProcessRequestVoteServerResponse(call->request, call->reply, peer_address);

  Logger::Debug("RequestVote call was received");
}

void RaftClient::HandleAppendEntriesReply(AsyncClientCall<protocol::raft::AppendEntries_Request,
      protocol::raft::AppendEntries_Response>* call) {
  if (!call->status.ok()) {
    Logger::Info("AppendEntries call failed unexpectedly");
    return;
  }

  std::string peer_address = call->ctx.peer().substr(5);
  m_ctx.ConcensusInstance()->ProcessAppendEntriesServerResponse(call->request, call->reply, peer_address);

  Logger::Debug("AppendEntries call was received");
}

// void RaftClient::HandleSetConfigurationReply(AsyncClientCall<protocol::raft::SetConfiguration_Request,
//     protocol::raft::SetConfiguration_Response>* call) {
//   if (!call->status.ok()) {
//     Logger::Info("SetConfiguration call failed unexpectedly");
//     return;
//   }
//
//   // m_ctx.ConcensusInstance()->ProcessSetConfigurationServerResponse(call->request, call->reply);
//
//   Logger::Debug("SetConfiguration response was received");
// }

// void RaftClient::HandleGetConfigurationReply(AsyncClientCall<protocol::raft::GetConfiguration_Request,
//     protocol::raft::GetConfiguration_Response>* call) {
//   if (!call->status.ok()) {
//     Logger::Info("GetConfiguration call failed unexpectedly");
//     return;
//   }
//
//   // m_ctx.ConcensusInstance()->ProcessGetConfigurationServerResponse(call->request, call->reply);
//
//   Logger::Debug("GetConfiguration response was received");
// }

}

