#include "concensus_module.h"
#include "global_ctx_manager.h"
#include "raft_client.h"

namespace raft {

ConcensusModule::ConcensusModule(GlobalCtxManager& ctx)
  : m_ctx(ctx)
  , m_executor(std::make_shared<Strand>())
  , m_vote("")
  , m_votes_received(0)
  , m_term(0)
  , m_state(RaftState::CANDIDATE) {
  m_election_timer = m_ctx.timer_queue->CreateTimer(
    150,
    m_executor,
    std::bind(&ConcensusModule::ElectionCallback, this, m_term));
  m_heartbeat_timer = m_ctx.timer_queue->CreateTimer(
    50,
    m_executor,
    std::bind(&ConcensusModule::HeartbeatCallback, this));

  ScheduleElection(m_term);
}

void ConcensusModule::ElectionCallback(const std::size_t term) {
  if (m_state != RaftState::CANDIDATE && m_state != RaftState::FOLLOWER) {
    Logger::Debug("Concensus module state invalid for election");
    return;
  }

  if (m_term != term) {
    Logger::Debug("Term changed from", term, "to", m_term);
    return;
  }

  m_state = RaftState::CANDIDATE;
  m_term++;
  std::size_t saved_term = m_term;
  m_vote = m_ctx.address;
  m_votes_received++;

  std::size_t last_log_index = 0;
  std::size_t last_log_term = 0;
  for (auto peer_id:m_ctx.peer_ids) {
    Logger::Debug("Sending RequestVote rpc to", peer_id);

    m_ctx.client.RequestVote(
        peer_id,
        saved_term,
        last_log_index,
        last_log_term);
  }

  ScheduleElection(saved_term);
}

void ConcensusModule::HeartbeatCallback() {
  if (m_state != RaftState::LEADER) {
    Logger::Debug("Invalid state for sending heartbeat");
    return;
  }

  std::size_t saved_term = m_term;

  std::size_t prev_log_index = 0;
  std::size_t prev_log_term = 0;
  std::size_t leader_commit = 0;
  for (auto peer_id:m_ctx.peer_ids) {
    Logger::Debug("Sending AppendEntries rpc to", peer_id);

    m_ctx.client.AppendEntries(
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

  m_state = RaftState::DEAD;
  Logger::Info("Node shutdown");
}

void ConcensusModule::ResetToFollower(const std::size_t term) {
  m_state = RaftState::FOLLOWER;
  m_term = term;
  m_vote = "";
  m_votes_received = 0;
  Logger::Debug("Reset to follower, term:", m_term);

  ScheduleElection(term);
}

void ConcensusModule::PromoteToLeader() {
  m_state = RaftState::LEADER;
  m_votes_received = 0;
  Logger::Debug("Promoted to leader, term:", m_term);

  m_election_timer->Cancel();

  ScheduleHeartbeat();
}

}

