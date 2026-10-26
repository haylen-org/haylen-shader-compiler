#pragma once

#include <condition_variable>
#include <cstddef>
#include <exception>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace haylen::shaderc {

// A fixed set of threads that run the tasks of one batch at a time.
class ThreadPool {
  public:
    // Zero threads start one thread per processor.
    explicit ThreadPool(unsigned count);
    ~ThreadPool();
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // Runs the task once for every index below the count and waits for all of them. A task that throws lets the others finish, and the exception of the lowest index is rethrown here.
    void run(std::size_t count, const std::function<void(std::size_t)>& task);

  private:
    std::vector<std::thread> threads;
    std::mutex mutex;
    std::condition_variable wake;
    std::condition_variable done;
    const std::function<void(std::size_t)>* current = nullptr;
    std::size_t total = 0;
    std::size_t next = 0;
    std::size_t finished = 0;
    std::vector<std::exception_ptr> failures;
    bool stopping = false;

    void work();
};

} // namespace haylen::shaderc
