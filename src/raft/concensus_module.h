#ifndef CONCENSUS_MODULE_H
#define CONCENSUS_MODULE_H

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "logger.h"
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

  void StateMachineInit(std::size_t delay = 0);

  std::size_t Term() const;

  RaftState State() const;

  void ResetToFollower(const std::size_t term);

  void PromoteToLeader();

private:
  void ElectionCallback(const std::size_t term);

  void HeartbeatCallback();

  void ScheduleElection(const std::size_t term);

  void ScheduleHeartbeat();

  void Shutdown();

private:
  GlobalCtxManager& m_ctx;
  std::mutex m_mutex;
  std::shared_ptr<core::DeadlineTimer> m_election_timer;
  std::shared_ptr<core::DeadlineTimer> m_heartbeat_timer;
  std::string m_vote;
  std::size_t m_votes_received;
  std::atomic<std::size_t> m_term;
  std::atomic<RaftState> m_state;
};

}

#endif

