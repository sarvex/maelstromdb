#include "async_executor.h"

namespace core {

AsyncExecutor::AsyncExecutor() 
  : m_abort(false) {
}

AsyncExecutor::~AsyncExecutor() {
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

ThreadPoolExecutor::ThreadPoolWorker::ThreadPoolWorker()
  : AsyncExecutor(), m_idle(false) {
  m_worker = std::thread([this] {
    EventLoop();
  });
}

void ThreadPoolExecutor::ThreadPoolWorker::Enqueue(const callback_t callback) {
  std::unique_lock<std::mutex> lock(m_lock);
  if (m_abort) {
    throw std::runtime_error("Worker shutdown");
  }

  UpdateWorkerThread(lock);

  m_request_queue.push(std::move(callback));
  lock.unlock();

  m_cond.notify_one();
}

void ThreadPoolExecutor::ThreadPoolWorker::Shutdown() {
  auto prev_state = m_abort.exchange(true);
  if (prev_state) {
    return;
  }

  std::unique_lock<std::mutex> lock(m_lock);
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

void ThreadPoolExecutor::ThreadPoolWorker::EventLoop() {
  while (true) {
    std::unique_lock<std::mutex> lock(m_lock);
    bool result = false;

    if (m_request_queue.empty()) {
      result = m_cond.wait_for(lock, std::chrono::seconds(5), [this] {
        return !m_request_queue.empty() || m_abort.load();
      });
    }

    if (m_abort.load() || !result) {
      m_idle = true;
      m_parent_pool->SetWorkerActivity(m_pool_index, false);
      return;
    }

    m_process_queue = std::move(m_request_queue);
    lock.unlock();

    m_parent_pool->SetWorkerActivity(m_pool_index, true);
    ProcessEvents();
  }
}

void ThreadPoolExecutor::ThreadPoolWorker::ProcessEvents() {
  BalanceWork();

  while (!m_process_queue.empty()) {
    auto callback = std::move(m_process_queue.front());
    m_process_queue.pop();

    if (m_abort.load()) {
      m_idle = true;
      return;
    }

    callback();
  }

  m_parent_pool->SetWorkerActivity(m_pool_index, false);
}

void ThreadPoolExecutor::ThreadPoolWorker::BalanceWork() {
  std::size_t task_count = m_process_queue.size();
  if (task_count <= 1) {
    return;
  }

  std::size_t max_idle_workers = std::min(m_parent_pool->PoolSize() - 1, task_count - 1);
  auto idle_markers = m_parent_pool->FindIdleWorkers(max_idle_workers);
  if (idle_markers.size() == 0) {
    return;
  }

  std::size_t new_tasks_per_worker = task_count/(idle_markers.size() + 1);
  for (auto& worker_index:idle_markers) {
    for (int i = 0; i < new_tasks_per_worker; i++) {
      m_parent_pool->Worker(worker_index).Enqueue(std::move(m_process_queue.front()));
      m_process_queue.pop();
    }
  }
}

void ThreadPoolExecutor::ThreadPoolWorker::UpdateWorkerThread(std::unique_lock<std::mutex>& lock) {
  assert(lock.owns_lock());
  if (!m_idle) {
    return;
  }

  auto prev_worker = std::move(m_worker);
  m_worker = std::thread([this] {
      EventLoop();
  });

  m_idle = false;
  lock.unlock();

  if (prev_worker.joinable()) {
    prev_worker.join();
  }
}

ThreadPoolExecutor::ThreadPoolExecutor(std::size_t pool_size)
  : AsyncExecutor(), m_pool_size(pool_size), m_round_robin_index(0) {
}

ThreadPoolExecutor::~ThreadPoolExecutor() {
  Shutdown();
}

void ThreadPoolExecutor::Enqueue(const callback_t callback) {
  std::size_t idle_worker_index = FindIdleWorker();
  if (idle_worker_index < PoolSize()) {
    m_workers[idle_worker_index].Enqueue(std::move(callback));
    return;
  }

  std::size_t next_worker_index = RoundRobinNext();
  m_workers[next_worker_index].Enqueue(std::move(callback));
}

void ThreadPoolExecutor::Shutdown() {
  auto abort = m_abort.exchange(true);
  if (abort) {
    return;
  }

  for (auto& worker:m_workers) {
    worker.Shutdown();
  }
}

std::size_t ThreadPoolExecutor::PoolSize() const {
  return m_pool_size;
}

ThreadPoolExecutor::ThreadPoolWorker& ThreadPoolExecutor::Worker(std::size_t index) {
  if (index >= PoolSize()) {
    throw std::out_of_range("Worker index out of bounds");
  }
  return m_workers[index];
}

std::size_t ThreadPoolExecutor::FindIdleWorker() const {
  for (std::size_t i = 0; i < PoolSize(); i++) {
    if (m_idle_markers[i].load()) {
      return i;
    }
  }
  return SIZE_MAX;
}

std::vector<std::size_t> ThreadPoolExecutor::FindIdleWorkers(std::size_t thread_count) const {
  std::vector<std::size_t> idle_workers;
  for (int i = 0; i < PoolSize() && idle_workers.size() < thread_count; i++) {
    if (m_idle_markers[i].load()) {
      idle_workers.push_back(i);
    }
  }
  return idle_workers;
}

void ThreadPoolExecutor::SetWorkerActivity(std::size_t index, bool is_active) {
  m_idle_markers[index].store(is_active);
}

std::size_t ThreadPoolExecutor::RoundRobinNext() {
  return m_round_robin_index.fetch_add(1) % PoolSize();
}

}

