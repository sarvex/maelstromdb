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

namespace timer {

class TimerQueue;

class TimerEvent {
public:
  using clock_type = std::chrono::high_resolution_clock;
  using time_point = std::chrono::time_point<clock_type>;
  using milliseconds = std::chrono::milliseconds;

public:
  TimerEvent(
      std::size_t delay,
      std::shared_ptr<AsyncExecutor> executor,
      std::shared_ptr<TimerQueue> timer_queue);

  virtual ~TimerEvent();

  virtual void Execute();

  virtual void SetCallback(std::function<void()> func);

  time_point Deadline() const;
  void SetDeadline(std::size_t delay); 

  void Cancel();
  bool Cancelled() const;

  std::size_t Delay() const;
  void SetDelay(std::size_t);

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
  std::size_t m_delay;
};

template <typename TCallable, typename... TArgs>
class TimerCallbackEvent : public TimerEvent {
public:
  TimerCallbackEvent(
      std::size_t delay,
      std::shared_ptr<AsyncExecutor> executor,
      std::shared_ptr<TimerQueue> timer_queue,
      TCallable&& callback,
      TArgs&&... args);

  void Execute() override;

  void SetCallback(std::function<void()> func) override;

private:
  std::function<void()> m_callback;
};

class DeadlineTimer {
public:
  DeadlineTimer(std::shared_ptr<TimerEvent> ctx);
  ~DeadlineTimer();

  void Cancel();

  void Reset();
  void Reset(std::size_t delay);
  template <typename TCallable, typename... TArgs>
  void Reset(TCallable&& callback, TArgs&&... args, std::size_t delay);

private:
  template <typename TCallable, typename... TArgs>
  std::function<void()> PackageCallback(TCallable&& callback, TArgs&&... args);

private:
  std::shared_ptr<TimerEvent> m_timer_ctx;
};

class TimerQueue {
public:
  using timer_set = std::multiset<std::shared_ptr<TimerEvent>>;
public:
  TimerQueue();

  ~TimerQueue();

  template <typename TCallable, typename... TArgs>
  std::shared_ptr<DeadlineTimer> CreateTimer(
      std::size_t delay,
      std::shared_ptr<AsyncExecutor> executor,
      TCallable&& callback,
      TArgs&&... args);

  void AddTimer(std::shared_ptr<TimerEvent> timer);

  void RemoveTimer(std::shared_ptr<TimerEvent> timer);

  void Shutdown();

  bool Empty() const;

private:
  void EventLoop();

  TimerEvent::time_point ProcessTimers();

  std::thread UpdateWorkerThread(std::unique_lock<std::mutex>& lock);

private:
  timer_set m_request_queue;
  timer_set m_process_queue;
  std::mutex m_lock;
  std::atomic<bool> m_abort;
  std::condition_variable m_cond;
  std::thread m_worker;
  bool m_idle;
};

template <typename TCallable, typename... TArgs>
TimerCallbackEvent<TCallable, TArgs...>::TimerCallbackEvent(
    std::size_t delay,
    std::shared_ptr<AsyncExecutor> executor,
    std::shared_ptr<TimerQueue> timer_queue,
    TCallable&& callback,
    TArgs&&... args)
  : TimerEvent(delay, std::move(executor), std::move(timer_queue)) {
  using CallbackReturnType = typename std::result_of<TCallable(TArgs...)>::type;

  auto task = std::make_shared<std::packaged_task<CallbackReturnType()>>(
      std::bind(std::forward(callback), std::forward(args)...));

  m_callback = [this, task = std::move(task)]() {
    (*task)();
  };
}

template <typename TCallable, typename... TArgs>
void TimerCallbackEvent<TCallable, TArgs...>::Execute() {
  if (Cancelled()) {
    return;
  }

  m_executor->Enqueue(m_callback);
}

template <typename TCallable, typename... TArgs>
void TimerCallbackEvent<TCallable, TArgs...>::SetCallback(std::function<void()> func) {
  m_callback = func;
}

template <typename TCallable, typename... TArgs>
void DeadlineTimer::Reset(TCallable&& callback, TArgs&&... args, std::size_t delay) {
  auto new_callback = PackageCallback(callback, args...);

  m_timer_ctx->SetCallback(new_callback);
  m_timer_ctx->SetDelay(delay);
  Reset();
}

template <typename TCallable, typename... TArgs>
std::function<void()> DeadlineTimer::PackageCallback(TCallable&& callback, TArgs&&... args) {
  using CallbackReturnType = typename std::result_of<TCallable(TArgs...)>::type;

  auto task = std::make_shared<std::packaged_task<CallbackReturnType()>>(
      std::bind(std::forward(callback), std::forward(args)...));
  auto packaged_callback = [this, task = std::move(task)]() {
    (*task)();
  };

  return packaged_callback;
}

template <typename TCallable, typename... TArgs>
std::shared_ptr<DeadlineTimer> TimerQueue::CreateTimer(
    std::size_t delay,
    std::shared_ptr<AsyncExecutor> executor,
    TCallable&& callback,
    TArgs&&... args) {
  auto ctx = std::make_shared<TimerCallbackEvent<TCallable, TArgs...>>(
      delay, executor, callback, args...);
  auto new_timer = std::make_shared<DeadlineTimer>(ctx);

  AddTimer(new_timer);

  return new_timer;
}

}

#endif

