#ifndef TIMER_H
#define TIMER_H

#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <future>
#include <memory>
#include <set>

#include "async_executor.h"
#include "logger.h"

namespace core {

class TimerQueue;

class TimerEvent {
public:
  using clock_type = std::chrono::high_resolution_clock;
  using time_point = std::chrono::time_point<clock_type>;
  using milliseconds = std::chrono::milliseconds;

public:
  TimerEvent(
      int delay,
      std::shared_ptr<AsyncExecutor> executor,
      std::shared_ptr<TimerQueue> timer_queue);

  virtual ~TimerEvent();

  virtual void Execute() = 0;

  virtual void SetCallback(std::function<void()>&& func) = 0;

  time_point Deadline() const;
  void SetDeadline(int delay); 

  void Cancel();
  bool Cancelled() const;

  int Delay() const;
  void SetDelay(int);

  void Reset();

  bool Expired(time_point time) const;

  std::shared_ptr<TimerQueue> TimerQueueInstance() const;
  std::shared_ptr<AsyncExecutor> ExecutorInstance() const;

  bool operator<(const TimerEvent& rhs);

protected:
  std::atomic<bool> m_cancelled;
  std::shared_ptr<AsyncExecutor> m_executor;
  std::shared_ptr<TimerQueue> m_request_queue;
  time_point m_deadline;
  int m_delay;
};

class TimerCallbackEvent : public TimerEvent {
public:
  TimerCallbackEvent(
      int delay,
      std::shared_ptr<AsyncExecutor> executor,
      std::shared_ptr<TimerQueue> timer_queue,
      std::function<void()>&& callback);

  void Execute() override;

  void SetCallback(std::function<void()>&& callback) override;

private:
  std::function<void()> m_callback;
};

class DeadlineTimer {
public:
  DeadlineTimer(std::shared_ptr<TimerEvent> ctx);
  ~DeadlineTimer();

  void Cancel();

  void Reset();
  void Reset(int delay);
  void Reset(std::function<void()>&& callback, int delay);

private:
  std::shared_ptr<TimerEvent> m_timer_ctx;
};

class TimerQueue : public std::enable_shared_from_this<TimerQueue> {
public:
  enum class TimerEventType {
    ADD,
    REMOVE
  };

  using event_queue = std::vector<std::pair<std::shared_ptr<TimerEvent>, TimerEventType>>;
  using timer_set = std::multiset<std::shared_ptr<TimerEvent>>;

public:
  TimerQueue();

  ~TimerQueue();

  std::shared_ptr<DeadlineTimer> CreateTimer(
      int delay,
      std::shared_ptr<AsyncExecutor> executor,
      std::function<void()>&& callback);

  void AddTimer(std::shared_ptr<TimerEvent> timer);

  void RemoveTimer(std::shared_ptr<TimerEvent> timer);

  void Shutdown();

  bool Empty() const;

private:
  void EventLoop();

  TimerEvent::time_point ProcessTimers(event_queue& events);

  void AddTimerInternal(std::shared_ptr<TimerEvent> new_timer);

  void RemoveTimerInternal(std::shared_ptr<TimerEvent> existing_timer);

  std::thread UpdateWorkerThread(std::unique_lock<std::mutex>& lock);

private:
  event_queue m_request_queue;
  timer_set m_process_queue;
  std::mutex m_lock;
  std::atomic<bool> m_abort;
  std::condition_variable m_cond;
  std::thread m_worker;
  bool m_idle;
};

}

#endif

