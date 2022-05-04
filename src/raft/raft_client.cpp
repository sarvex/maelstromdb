#include "raft_client.h"
#include "global_ctx_manager.h"

namespace raft {

RaftClient::RaftClient(GlobalCtxManager& ctx)
  : m_ctx(ctx) {
}

void RaftClient::RequestVote(
    const std::string& peer_id,
    const std::size_t term,
    const std::size_t last_log_index,
    const std::size_t last_log_term) {
  protocol::raft::RequestVote_Request args;
  args.set_term(term);
  args.set_candidateid(m_ctx.address);
  args.set_lastlogindex(last_log_index);
  args.set_lastlogterm(last_log_term);
}

void RaftClient::AppendEntries(
    const std::string& peer_id,
    const std::size_t term,
    const std::size_t prev_log_index,
    const std::size_t prev_log_term,
    const std::size_t leader_commit) {
  protocol::raft::AppendEntries_Request args;
  args.set_term(term);
  args.set_leaderid(m_ctx.address);
  args.set_prevlogindex(prev_log_index);
  args.set_prevlogterm(prev_log_term);
  args.set_leadercommit(leader_commit);
}

}

