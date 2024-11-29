#pragma once

#include <functional>
#include <mutex>
#include <thread>

namespace luau::debugger {
class TaskPool {
 public:
  using Task = std::function<void()>;
  TaskPool(std::thread::id main_thread_id) : main_thread_id_(main_thread_id) {}
  void post(Task task) {
    if (isMainThread()) {
      task();
      return;
    }
    std::scoped_lock lock(mutex_);
    tasks_.push_back(std::move(task));
  }

  void process() {
    std::vector<Task> tasks;
    {
      std::scoped_lock lock(mutex_);
      std::swap(tasks_, tasks);
    }

    for (const auto& task : tasks)
      task();
  }

 private:
  bool isMainThread() const {
    return std::this_thread::get_id() == main_thread_id_;
  }

 private:
  std::thread::id main_thread_id_;
  std::vector<Task> tasks_;
  std::mutex mutex_;
};
}  // namespace luau::debugger