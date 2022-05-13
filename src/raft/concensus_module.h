#ifndef CONCENSUS_MODULE_H
#define CONCENSUS_MODULE_H

#include <grpcpp/grpcpp.h>
#include <algorithm>
#include <functional>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "async_executor.h"
#include "logger.h"
#include "raft.grpc.pb.h"
#include "timer.h"

namespace raft {

class GlobalCtxManager;

class ConcensusModule {
public:
  enum class RaftState {
    LEADER,
    CANDIDATE,
    FOLLOWER,
    DEAD
  };

public:
  ConcensusModule(GlobalCtxManager& ctx);

  ConcensusModule(const ConcensusModule&) = delete;
  ConcensusModule& operator=(const ConcensusModule&) = delete;

  void StateMachineInit(std::size_t delay = 0);

  std::size_t Term() const;

  RaftState State() const;

  void ResetToFollower(const std::size_t term);

  void PromoteToLeader();

  std::tuple<protocol::raft::RequestVote_Response, grpc::Status> ProcessRequestVoteClientRequest(
      protocol::raft::RequestVote_Request& request);

  void ProcessRequestVoteServerResponse(
      protocol::raft::RequestVote_Request& request,
      protocol::raft::RequestVote_Response& reply);

  std::tuple<protocol::raft::AppendEntries_Response, grpc::Status> ProcessAppendEntriesClientRequest(
      protocol::raft::AppendEntries_Request& request);

  void ProcessAppendEntriesServerResponse(
      protocol::raft::AppendEntries_Request& request,
      protocol::raft::AppendEntries_Response& reply,
      const std::string& address);

private:
  void ElectionCallback(const std::size_t term);

  void HeartbeatCallback();

  void ScheduleElection(const std::size_t term);

  void ScheduleHeartbeat();

  bool CheckQuorum(const std::size_t votes) const;

  void StoreState() const;

  void Shutdown();

private:
  GlobalCtxManager& m_ctx;
  std::shared_ptr<core::AsyncExecutor> m_timer_executor;
  std::shared_ptr<core::DeadlineTimer> m_election_timer;
  std::shared_ptr<core::DeadlineTimer> m_heartbeat_timer;
  std::string m_vote;
  std::size_t m_votes_received;
  std::atomic<std::size_t> m_term;
  std::atomic<RaftState> m_state;
  std::atomic<ssize_t> m_last_applied;
  std::atomic<ssize_t> m_commit_index;
  std::unordered_map<std::string, std::size_t> m_next_index;
  std::unordered_map<std::string, ssize_t> m_match_index;
};

}

#endif

