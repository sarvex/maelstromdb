#include "async_executor.h"

AsyncExecutor::AsyncExecutor() 
  : m_abort(false) {
}

AsyncExecutor::~AsyncExecutor() {
}

void AsyncExecutor::Enqueue(callback_t&& callback) {
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

void Strand::Enqueue(callback_t&& callback) {
  std::unique_lock<std::mutex> lock(m_lock);
  if (m_abort) {
    Logger::Error("Unable to execute callback, executor process has been aborted");
    throw std::runtime_error("Executor process has been aborted");
  }

  auto prev_thread = UpdateWorkerThread(lock);
  m_request_queue.push(std::move(callback));
  lock.unlock();

  m_cond.notify_one();

  if (prev_thread.joinable()) {
    prev_thread.join();
  }
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
      auto result = m_cond.wait_for(lock, std::chrono::seconds(5), [this] {
        return !m_request_queue.empty() || m_abort.load();
      });

      if (!result) {
        m_idle = true;
        return;
      }
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

std::thread Strand::UpdateWorkerThread(std::unique_lock<std::mutex>& lock) {
  assert(lock.owns_lock());
  if (!m_idle) {
    return {};
  }

  auto prev_thread = std::move(m_worker);

  m_worker = std::thread([this] {
    EventLoop();
  });
  m_idle = false;

  return prev_thread;
}

