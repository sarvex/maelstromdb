#ifndef RAFT_CLIENT_H
#define RAFT_CLIENT_H

#include <grpcpp/grpcpp.h>

#include "logger.h"
#include "raft.grpc.pb.h"

namespace raft {

class GlobalCtxManager;

class RaftClient {
public:
  RaftClient(GlobalCtxManager& ctx);

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

private:
  GlobalCtxManager& m_ctx;
};

}

#endif

