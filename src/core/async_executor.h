#ifndef ASYNC_EXECUTOR
#define ASYNC_EXECUTOR

#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <queue>

#include "logger.h"

namespace core {

class AsyncExecutor {
public:
  using callback_t = std::function<void()>;
public:
  AsyncExecutor();
  virtual ~AsyncExecutor();

  virtual void Enqueue(const callback_t callback) = 0;

  virtual void Shutdown() = 0;

protected:
  std::atomic<bool> m_abort;
  std::mutex m_lock;
  std::condition_variable m_cond;
  std::queue<callback_t> m_request_queue;
  std::queue<callback_t> m_process_queue;
};

class Strand : public AsyncExecutor {
public:
  Strand();
  ~Strand();

  void Enqueue(const callback_t callback) override;

  void Shutdown() override;

private:
  void EventLoop();
  void ProcessEvents();

  std::thread UpdateWorkerThread(std::unique_lock<std::mutex>& lock);

private:
  bool m_idle;
  std::thread m_worker;
};

class ThreadPoolExecutor : public AsyncExecutor {
private:
  class ThreadPoolWorker : public AsyncExecutor {
  public:
    ThreadPoolWorker();

    void Enqueue(const callback_t callback) override;

    void Shutdown() override;

  private:
    void EventLoop();
    void ProcessEvents();

    void BalanceWork();

    void UpdateWorkerThread(std::unique_lock<std::mutex>& lock);

  private:
    bool m_idle;
    std::thread m_worker;
    std::size_t m_pool_index;
    std::shared_ptr<ThreadPoolExecutor> m_parent_pool;
  };

public:
  ThreadPoolExecutor(std::size_t pool_size);
  ~ThreadPoolExecutor();

  void Enqueue(const callback_t callback) override;

  void Shutdown() override;

  std::size_t PoolSize() const;

  ThreadPoolWorker& Worker(std::size_t index);

private:
  std::size_t FindIdleWorker() const;
  std::vector<std::size_t> FindIdleWorkers(std::size_t thread_count) const;

  void SetWorkerActivity(std::size_t index, bool is_active);

  std::size_t RoundRobinNext();

private:
  std::size_t m_pool_size;
  std::atomic<std::size_t> m_round_robin_index;
  std::vector<ThreadPoolWorker> m_workers;
  std::vector<std::atomic<bool>> m_idle_markers;
};

}

#endif

