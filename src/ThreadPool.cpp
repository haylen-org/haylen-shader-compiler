#include "ThreadPool.hpp"

#include <algorithm>

namespace haylen::shaderc {

ThreadPool::ThreadPool(unsigned count) {
    const unsigned size = count == 0 ? std::max(1U, std::thread::hardware_concurrency()) : count;
    threads.reserve(size);
    for (unsigned index = 0; index < size; ++index) {
        threads.emplace_back(&ThreadPool::work, this);
    }
}

ThreadPool::~ThreadPool() {
    {
        const std::scoped_lock lock(mutex);
        stopping = true;
    }
    wake.notify_all();
    for (std::thread& thread : threads) {
        thread.join();
    }
}

void ThreadPool::run(std::size_t count, const std::function<void(std::size_t)>& task) {
    if (count == 0) {
        return;
    }
    {
        const std::scoped_lock lock(mutex);
        current = &task;
        total = count;
        next = 0;
        finished = 0;
        failures.assign(count, nullptr);
    }
    wake.notify_all();

    std::unique_lock lock(mutex);
    done.wait(lock, [this] { return finished == total; });
    current = nullptr;
    total = 0;
    next = 0;
    const auto failure = std::ranges::find_if(failures, [](const std::exception_ptr& value) { return value != nullptr; });
    if (failure != failures.end()) {
        std::rethrow_exception(*failure);
    }
}

void ThreadPool::work() {
    std::unique_lock lock(mutex);
    while (true) {
        wake.wait(lock, [this] { return stopping || next < total; });
        if (stopping) {
            return;
        }

        const std::size_t index = next++;
        const std::function<void(std::size_t)>& task = *current;
        lock.unlock();
        std::exception_ptr failure;
        try {
            task(index);
        } catch (...) {
            failure = std::current_exception();
        }
        lock.lock();

        failures[index] = failure;
        if (++finished == total) {
            done.notify_all();
        }
    }
}

} // namespace haylen::shaderc
