#include "concensus_module.h"
#include "global_ctx_manager.h"
#include "raft_client.h"

namespace raft {

ConcensusModule::ConcensusModule(GlobalCtxManager& ctx)
  : m_ctx(ctx)
  , m_vote("")
  , m_votes_received(0)
  , m_term(0)
  , m_state(RaftState::CANDIDATE) {
}

void ConcensusModule::StateMachineInit(std::size_t delay) {
  std::this_thread::sleep_for(std::chrono::seconds(delay));
  m_election_timer = m_ctx.TimerQueueInstance()->CreateTimer(
      150,
      m_ctx.ExecutorInstance(),
      std::bind(&ConcensusModule::ElectionCallback, this, Term()));
  m_heartbeat_timer = m_ctx.TimerQueueInstance()->CreateTimer(
      50,
      m_ctx.ExecutorInstance(),
      std::bind(&ConcensusModule::HeartbeatCallback, this));

  Logger::Debug("Starting election");
  ScheduleElection(Term());
}

std::size_t ConcensusModule::Term() const {
  return m_term.load();
}

ConcensusModule::RaftState ConcensusModule::State() const {
  return m_state.load();
}

void ConcensusModule::ElectionCallback(const std::size_t term) {
  if (State() != RaftState::CANDIDATE && State() != RaftState::FOLLOWER) {
    Logger::Debug("Concensus module state invalid for election");
    return;
  }

  if (Term() != term) {
    Logger::Debug("Term changed from", term, "to", Term());
    return;
  }

  m_state.store(RaftState::CANDIDATE);
  m_term++;
  std::size_t saved_term = Term();
  m_vote = m_ctx.address;
  m_votes_received++;

  std::size_t last_log_index = 0;
  std::size_t last_log_term = 0;
  for (auto peer_id:m_ctx.peer_ids) {
    Logger::Debug("Sending RequestVote rpc to", peer_id);

    m_ctx.ClientInstance()->RequestVote(
        peer_id,
        saved_term,
        last_log_index,
        last_log_term);
  }

  ScheduleElection(saved_term);
}

void ConcensusModule::HeartbeatCallback() {
  if (State() != RaftState::LEADER) {
    Logger::Debug("Invalid state for sending heartbeat");
    return;
  }

  std::size_t saved_term = Term();

  std::size_t prev_log_index = 0;
  std::size_t prev_log_term = 0;
  std::size_t leader_commit = 0;
  for (auto peer_id:m_ctx.peer_ids) {
    Logger::Debug("Sending AppendEntries rpc to", peer_id);

    m_ctx.ClientInstance()->AppendEntries(
        peer_id,
        saved_term,
        prev_log_index,
        prev_log_term,
        leader_commit);
  }

  ScheduleHeartbeat();
}

void ConcensusModule::ScheduleElection(const std::size_t term) {
  int random_timeout = std::rand()%151 + 150;
  Logger::Debug("Election timer created:", random_timeout, "ms");
  m_election_timer->Reset(
      std::bind(&ConcensusModule::ElectionCallback, this, term),
      random_timeout);
}

void ConcensusModule::ScheduleHeartbeat() {
  Logger::Debug("Heartbeat timer created");
  m_heartbeat_timer->Reset();
}

void ConcensusModule::Shutdown() {
  m_election_timer->Cancel();
  m_heartbeat_timer->Cancel();

  m_state.store(RaftState::DEAD);
  Logger::Info("Node shutdown");
}

void ConcensusModule::ResetToFollower(const std::size_t term) {
  m_state.store(RaftState::FOLLOWER);
  m_term.store(term);
  m_vote = "";
  m_votes_received = 0;
  Logger::Debug("Reset to follower, term:", Term());

  ScheduleElection(term);
}

void ConcensusModule::PromoteToLeader() {
  m_state.store(RaftState::LEADER);
  m_votes_received = 0;
  Logger::Debug("Promoted to leader, term:", Term());

  m_election_timer->Cancel();

  ScheduleHeartbeat();
}

}

