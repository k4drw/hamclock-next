#pragma once

#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class WorkerService {
public:
  static WorkerService &getInstance();

  ~WorkerService();

  // Submit a task to be executed by a worker thread.
  void submitTask(std::function<void()> task);

  // Stop all worker threads.
  void stop();

private:
  WorkerService();
  WorkerService(const WorkerService &) = delete;
  WorkerService &operator=(const WorkerService &) = delete;

  void workerLoop();

  bool shouldStop_ = false;
  std::vector<std::thread> workers_;
  std::queue<std::function<void()>> tasks_;
  std::mutex queueMutex_;
  std::condition_variable condition_;
};
