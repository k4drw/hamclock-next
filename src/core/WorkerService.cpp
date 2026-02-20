#include "WorkerService.h"
#include "Logger.h"
#include <pthread.h> // For setting thread priority

WorkerService &WorkerService::getInstance() {
  static WorkerService instance;
  return instance;
}

WorkerService::WorkerService() {
  const size_t num_threads = 2; // A small pool for embedded devices
  workers_.reserve(num_threads);
  for (size_t i = 0; i < num_threads; ++i) {
    workers_.emplace_back([this] { this->workerLoop(); });

#ifdef __linux__
    // Set thread priority to SCHED_IDLE to avoid interfering with UI thread
    pthread_t native_handle = workers_.back().native_handle();
    sched_param sch_params;
    sch_params.sched_priority = 0; // Priority is not used for SCHED_IDLE
    int ret = pthread_setschedparam(native_handle, SCHED_IDLE, &sch_params);
    if (ret != 0) {
      // Non-fatal, but log it. May fail if process lacks permissions.
      LOG_W("WorkerService", "Failed to set thread priority to IDLE: {}", std::strerror(ret));
    } else {
      LOG_D("WorkerService", "Worker thread {} set to IDLE priority.", i);
    }
#endif
  }
}

WorkerService::~WorkerService() {
  stop();
}

void WorkerService::workerLoop() {
  while (true) {
    std::function<void()> task;
    {
      std::unique_lock<std::mutex> lock(queueMutex_);
      condition_.wait(lock, [this] { return shouldStop_ || !tasks_.empty(); });

      if (shouldStop_ && tasks_.empty()) {
        return;
      }

      task = std::move(tasks_.front());
      tasks_.pop();
    }
    try {
        task();
    } catch (const std::exception& e) {
        LOG_E("WorkerService", "Exception in background task: {}", e.what());
    } catch (...) {
        LOG_E("WorkerService", "Unknown exception in background task.");
    }
  }
}

void WorkerService::submitTask(std::function<void()> task) {
  {
    std::unique_lock<std::mutex> lock(queueMutex_);
    if (shouldStop_) {
      return; // Don't accept new tasks if shutting down
    }
    tasks_.push(std::move(task));
  }
  condition_.notify_one();
}

void WorkerService::stop() {
  {
    std::unique_lock<std::mutex> lock(queueMutex_);
    if (shouldStop_) {
        return;
    }
    shouldStop_ = true;
  }
  condition_.notify_all();
  for (std::thread &worker : workers_) {
    if (worker.joinable()) {
      worker.join();
    }
  }
}
