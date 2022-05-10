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
  virtual void EventLoop() = 0;

  virtual void ProcessEvents() = 0; 

protected:
  std::mutex m_lock;
  std::atomic<bool> m_abort;
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
  void EventLoop() override;

  void ProcessEvents() override;

  std::thread UpdateWorkerThread(std::unique_lock<std::mutex>& lock);

private:
  bool m_idle;
  std::condition_variable m_cond;
  std::thread m_worker;
};

// TODO: Design thread pool
class PoolExecutor : public AsyncExecutor {
};

}

#endif

