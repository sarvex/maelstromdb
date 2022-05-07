#ifndef RAFT_CLIENT_H
#define RAFT_CLIENT_H

#include <grpcpp/grpcpp.h>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "logger.h"
#include "raft.grpc.pb.h"

namespace raft {

class GlobalCtxManager;

class RaftClient {
public:
  using stub_map = std::unordered_map<std::string, std::unique_ptr<protocol::raft::RaftService::Stub>>;

  enum class CommandID {
    REQUEST_VOTE,
    APPEND_ENTRIES
  };

  struct Tag {
    void* call;
    CommandID id;
  };

public:
  RaftClient(GlobalCtxManager& ctx);

  void ClientInit();

  void RequestVote(
      const std::string& peer_id,
      const std::size_t term,
      const std::size_t last_log_index,
      const std::size_t last_log_term);

  void AppendEntries(
      const std::string& peer_id,
      const std::size_t term,
      const std::size_t prev_log_index,
      const std::size_t prev_log_term,
      const std::size_t leader_commit);

  void AsyncCompleteRPC();

private:
  template <typename RequestType, typename ResponseType>
  struct AsyncClientCall {
    RequestType request;
    ResponseType reply;
    grpc::ClientContext ctx;
    grpc::Status status;
    std::unique_ptr<grpc::ClientAsyncResponseReader<ResponseType>> response_reader;
  };

  void HandleRequestVoteReply(AsyncClientCall<protocol::raft::RequestVote_Request,
      protocol::raft::RequestVote_Response>* call);

  void HandleAppendEntriesReply(AsyncClientCall<protocol::raft::AppendEntries_Request,
      protocol::raft::AppendEntries_Response>* call);

private:
  GlobalCtxManager& m_ctx;
  stub_map m_stubs;
  grpc::CompletionQueue m_cq;
};

}

#endif

