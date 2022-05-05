#include "async_executor.h"

AsyncExecutor::AsyncExecutor() 
  : m_abort(false) {
}

AsyncExecutor::~AsyncExecutor() {
}

void AsyncExecutor::Enqueue(callback_t callback) {
}

void AsyncExecutor::Shutdown() {
}

void AsyncExecutor::EventLoop() {
}

void AsyncExecutor::ProcessEvents() {
}

Strand::Strand()
  : AsyncExecutor(), m_idle(false) {
  m_worker = std::thread([this] {
    EventLoop();
  });
}

Strand::~Strand() {
  Shutdown();
}

void Strand::Enqueue(callback_t callback) {
  std::unique_lock<std::mutex> lock(m_lock);
  if (m_abort) {
    Logger::Error("Unable to execute callback, executor process has been aborted");
    throw std::runtime_error("Executor process has been aborted");
  }

  m_request_queue.push(std::move(callback));
  lock.unlock();

  m_cond.notify_one();
}

void Strand::Shutdown() {
  const auto prev_state = m_abort.exchange(true);
  if (prev_state) {
    return;
  }

  if (!m_worker.joinable()) {
    return;
  }

  std::queue<callback_t> temp_request_queue{};
  std::queue<callback_t> temp_process_queue{};

  {
    std::unique_lock<std::mutex> lock(m_lock);
    temp_request_queue = std::move(m_request_queue);
    temp_process_queue = std::move(m_process_queue);
  }

  m_cond.notify_one();
  m_worker.join();
}

void Strand::EventLoop() {
  while (true) {
    std::unique_lock<std::mutex> lock(m_lock);
    if (m_request_queue.empty()) {
      m_cond.wait(lock, [this] {
        return !m_request_queue.empty() || m_abort.load();
      });
    }

    if (m_abort.load()) {
      return;
    }

    m_process_queue = std::move(m_request_queue);
    lock.unlock();

    ProcessEvents();
  }
}

void Strand::ProcessEvents() {
  while (!m_process_queue.empty()) {
    auto callback = std::move(m_process_queue.front());
    m_process_queue.pop();

    if (m_abort.load()) {
      return;
    }

    callback();
  }
}

