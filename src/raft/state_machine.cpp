#include "state_machine.h"

namespace raft {

StateMachine::StateMachine(std::shared_ptr<SessionCache> sessions, std::shared_ptr<InmemoryStore> store)
  : m_last_applied(-1), m_sessions(sessions), m_store(store) {
}

int StateMachine::LastApplied() const {
  return m_last_applied.load();
}

void StateMachine::IncrementLastApplied() {
  m_last_applied++;
}

std::string StateMachine::ApplyCommand(int log_index, protocol::log::LogEntry &log_entry) {
  switch (log_entry.type()) {
    case protocol::log::LogOpCode::NO_OP: {
      break;
    }
    case protocol::log::LogOpCode::REGISTER_CLIENT: {
      m_sessions->AddSession(log_index);
      break;
    }
    case protocol::log::LogOpCode::DATA: {
      std::string command = log_entry.data();
      int split_pos = command.find(':');
      std::string key = command.substr(0, split_pos);
      std::string val = command.substr(split_pos + 1);
      m_store->Write(key, val);
      break;
    }
    default: {
    }
  }
  IncrementLastApplied();
  return "SUCCESS";
}

}

