#ifndef CONCENSUS_MODULE_H
#define CONCENSUS_MODULE_H

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "logger.h"
#include "timer.h"

namespace raft {

class GlobalCtxManager;

class ConcensusModule {
public:
  ConcensusModule(GlobalCtxManager& ctx);

  enum class RaftState {
    LEADER,
    CANDIDATE,
    FOLLOWER,
    DEAD
  };

private:
  void ElectionCallback(const std::size_t term);

  void HeartbeatCallback();

  void ScheduleElection(const std::size_t term);

  void ScheduleHeartbeat();

  void Shutdown();

  void ResetToFollower(const std::size_t term);

  void PromoteToLeader();

private:
  GlobalCtxManager& m_ctx;
  std::shared_ptr<timer::DeadlineTimer> m_election_timer;
  std::shared_ptr<timer::DeadlineTimer> m_heartbeat_timer;
  std::shared_ptr<AsyncExecutor> m_executor;
  std::string m_vote;
  std::size_t m_votes_received;
  std::size_t m_term;
  RaftState m_state;
};

}

#endif

