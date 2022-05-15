#include "concensus_module.h"
#include "global_ctx_manager.h"
#include "raft_client.h"
#include "storage.h"

namespace raft {

ConcensusModule::ConcensusModule(GlobalCtxManager& ctx)
  : m_ctx(ctx)
  , m_vote("")
  , m_votes_received(0)
  , m_term(0)
  , m_state(RaftState::CANDIDATE)
  , m_last_applied(-1)
  , m_commit_index(-1)
  , m_timer_executor(std::make_shared<core::Strand>()) {
}

void ConcensusModule::StateMachineInit(std::size_t delay) {
  for (auto peer:m_ctx.peer_ids) {
    m_next_index[peer] = 0;
    m_match_index[peer] = -1;
  }

  auto [metadata, is_valid] = m_ctx.LogInstance()->Metadata();
  if (is_valid) {
    m_term.store(metadata.term());
    m_vote = metadata.vote();
  }

  std::this_thread::sleep_for(std::chrono::seconds(delay));
  m_election_timer = m_ctx.TimerQueueInstance()->CreateTimer(
      150,
      m_timer_executor,
      std::bind(&ConcensusModule::ElectionCallback, this, Term()));
  m_heartbeat_timer = m_ctx.TimerQueueInstance()->CreateTimer(
      50,
      m_timer_executor,
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
  m_votes_received = 1;

  StoreState();

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
  std::random_device rd; // obtain a random number from hardware
  std::mt19937 gen(rd()); // seed the generator
  std::uniform_int_distribution<> distr(150, 300);
  int random_timeout = distr(gen);
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

  m_heartbeat_timer->Cancel();

  StoreState();

  ScheduleElection(term);
}

void ConcensusModule::PromoteToLeader() {
  m_state.store(RaftState::LEADER);
  m_votes_received = 0;
  Logger::Debug("Promoted to leader, term:", Term());

  m_election_timer->Cancel();

  ScheduleHeartbeat();
}

bool ConcensusModule::CheckQuorum(const std::size_t votes) const {
  return votes*2 > m_ctx.peer_ids.size() + 1;
}

void ConcensusModule::StoreState() const {
  protocol::log::LogMetadata metadata;
  metadata.set_term(Term());
  metadata.set_vote(m_vote);
  m_ctx.LogInstance()->SetMetadata(metadata);
}

std::tuple<protocol::raft::RequestVote_Response, grpc::Status> ConcensusModule::ProcessRequestVoteClientRequest(
    protocol::raft::RequestVote_Request& request) {
  protocol::raft::RequestVote_Response reply;

  if (State() == RaftState::DEAD) {
    return std::make_tuple(reply, grpc::Status::CANCELLED);
  }

  if (request.term() > Term()) {
    Logger::Debug("Term out of date in RequestVote RPC, changed from", Term(), "to", request.term());
    ResetToFollower(request.term());
  }

  auto last_log_index = m_ctx.LogInstance()->LastLogIndex();
  auto last_log_term = m_ctx.LogInstance()->LastLogTerm();

  if (request.term() == Term() &&
      (m_vote == "" || m_vote == request.candidateid()) &&
      (request.lastlogterm() > last_log_term ||
       (request.lastlogterm() == last_log_term && request.lastlogindex() >= last_log_index))) {
    reply.set_votegranted(true);
    m_vote = request.candidateid();

    ScheduleElection(request.term());
  } else {
    reply.set_votegranted(false);
  }

  reply.set_term(Term());
  return std::make_tuple(reply, grpc::Status::OK);
}

void ConcensusModule::ProcessRequestVoteServerResponse(
    protocol::raft::RequestVote_Request& request,
    protocol::raft::RequestVote_Response& reply) {
  if (State() != RaftState::CANDIDATE) {
    Logger::Debug("Node changed state while waiting for RequestVote reply");
    return;
  }

  if (reply.term() > request.term()) {
    Logger::Debug("Term out of date in election reply, changed from", request.term(), "to", reply.term());
    ResetToFollower(reply.term());
    return;
  } else if (reply.term() == request.term()) {
    if (reply.votegranted()) {
      m_votes_received++;

      if (CheckQuorum(m_votes_received)) {
        Logger::Debug("Wins election with", m_votes_received, "votes");
        PromoteToLeader();
        return;
      }
    }
  }
}

std::tuple<protocol::raft::AppendEntries_Response, grpc::Status> ConcensusModule::ProcessAppendEntriesClientRequest(
    protocol::raft::AppendEntries_Request& request) {
  protocol::raft::AppendEntries_Response reply;

  if (State() == RaftState::DEAD) {
    return std::make_tuple(reply, grpc::Status::CANCELLED);
  }

  if (request.term() > Term()) {
    Logger::Debug("Term out of date in AppendEntries RPC, changed from", Term(), "to", request.term());
    ResetToFollower(request.term());
  }

  bool success = false;
  if (request.term() == Term()) {
    if (State() != RaftState::FOLLOWER) {
      ResetToFollower(request.term());
    } else {
      ScheduleElection(request.term());
    }

    if (request.prevlogindex() == -1 ||
        (request.prevlogindex() < m_ctx.LogInstance()->LogSize() &&
         request.prevlogterm() == m_ctx.LogInstance()->Entry(request.prevlogindex()).term())) {
      success = true;

      std::size_t log_insert_index = request.prevlogindex() + 1;
      std::size_t new_entries_index = 0;

      while (log_insert_index < m_ctx.LogInstance()->LogSize() &&
          new_entries_index < request.entries().size() &&
          m_ctx.LogInstance()->Entry(log_insert_index).term() == request.entries()[new_entries_index].term()) {
        log_insert_index++;
        new_entries_index++;
      }

      if (new_entries_index < request.entries().size()) {
        std::vector<protocol::log::LogEntry> new_entries(request.entries().begin() + new_entries_index, request.entries().end());
        m_ctx.LogInstance()->Append(new_entries);
      }

      if (request.leadercommit() > m_commit_index) {
        std::size_t new_commit_index = std::min((std::size_t)request.leadercommit(), m_ctx.LogInstance()->LogSize());
        m_commit_index.store(new_commit_index);
        Logger::Debug("Setting commit index =", new_commit_index);

        std::size_t new_last_applied = m_last_applied;
        std::vector<protocol::log::LogEntry> uncommited_entries;
        while (new_last_applied < new_commit_index) {
          new_last_applied++;
          auto uncommited_entry = m_ctx.LogInstance()->Entry(new_last_applied);
          uncommited_entries.push_back(uncommited_entry);
        }
        m_last_applied.store(new_last_applied);

        // TODO: Commit new uncommited entries
      }
    }
  }

  reply.set_term(Term());
  reply.set_success(success);
  return std::make_tuple(reply, grpc::Status::OK);
}

void ConcensusModule::ProcessAppendEntriesServerResponse(
    protocol::raft::AppendEntries_Request& request,
    protocol::raft::AppendEntries_Response& reply,
    const std::string& address) {
  if (reply.term() > request.term()) {
    Logger::Debug("Term out of date in heartbeat reply, changed from", request.term(), "to", reply.term());
    ResetToFollower(reply.term());
  }

  if (State() == RaftState::LEADER && reply.term() == Term()) {
    int next = m_next_index[address];
    if (reply.success()) {
      m_next_index[address] = next + request.entries().size();
      m_match_index[address] = next + request.entries().size() - 1;
      Logger::Debug("AppendEntries reply from", address, "successful: next_index =", m_next_index[address], "match_index =", m_match_index[address]);

      std::size_t saved_commit_index = m_commit_index;
      std::size_t log_size = m_ctx.LogInstance()->LogSize();
      auto log_entries = m_ctx.LogInstance()->Entries(saved_commit_index + 1, log_size);
      std::size_t new_commit_index = saved_commit_index;
      for (int i = saved_commit_index + 1; i < log_size; i++) {
        if (log_entries[i - saved_commit_index - 1].term() == Term()) {
          int match_count = 1;
          for (auto peer:m_ctx.peer_ids) {
            if (m_match_index[peer] >= i) {
              match_count++;
            }
          }

          if (CheckQuorum(match_count)) {
            new_commit_index = i;
          }
        }
      }

      if (new_commit_index != saved_commit_index) {
        m_commit_index.store(new_commit_index);
        Logger::Debug("Leader set commit_index =", new_commit_index);

        std::size_t new_last_applied = m_last_applied;
        std::vector<protocol::log::LogEntry> uncommited_entries;
        while (new_last_applied < new_commit_index) {
          new_last_applied++;

          auto uncommited_entry = m_ctx.LogInstance()->Entry(new_last_applied);
          uncommited_entries.push_back(uncommited_entry);
        }
        m_last_applied.store(new_last_applied);

        // TODO: Commit new uncommited entries
      } else {
        m_next_index[address] = next - 1;
        Logger::Debug("AppendEntries reply from", address, "unsuccessful: next_index =", next);
      }
    }
  }
}

}

