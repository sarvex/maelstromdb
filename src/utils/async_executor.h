#ifndef ASYNC_EXECUTOR
#define ASYNC_EXECUTOR

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>

class AsyncExecutor {
public:
  AsyncExecutor();
  virtual ~AsyncExecutor();

  void Enqueue(std::function<void()> callback);

  void Shutdown();

  void WaitForTask();

private:
  void Execute();

protected:
  std::mutex m_task_lock;
  std::condition_variable m_cond;
  std::atomic<bool> m_cancelled;
  std::queue<std::function<void()>> m_callback_queue;
};

class SingleExecutor : public AsyncExecutor {
  SingleExecutor();
};

class PoolExecutor : public AsyncExecutor {
};

#endif

