#pragma once

/**
 * ThreadPool.hpp
 * 
 * High-performance thread pool for parallel task execution.
 * Used for downloads, file operations, and background processing.
 */

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <atomic>
#include <stdexcept>

namespace nexus::core {

/**
 * ThreadPool - Work-stealing thread pool
 * 
 * Features:
 * - Configurable thread count
 * - Task priorities
 * - Future-based results
 * - Graceful shutdown
 */
class ThreadPool {
public:
    /**
     * Constructor
     * @param numThreads Number of worker threads (0 = hardware concurrency)
     */
    explicit ThreadPool(size_t numThreads = 0) 
        : m_stop(false), m_activeJobs(0) {
        
        if (numThreads == 0) {
            numThreads = std::thread::hardware_concurrency();
            if (numThreads == 0) numThreads = 4; // Fallback
        }
        
        m_workers.reserve(numThreads);
        
        for (size_t i = 0; i < numThreads; ++i) {
            m_workers.emplace_back([this] {
                workerLoop();
            });
        }
    }
    
    /**
     * Destructor - waits for all tasks to complete
     */
    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_stop = true;
        }
        
        m_condition.notify_all();
        
        for (auto& worker : m_workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }
    
    // Disable copy and move
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;
    
    /**
     * Submit a task for execution
     * @param f Function to execute
     * @param args Function arguments
     * @return Future for the result
     */
    template<class F, class... Args>
    auto submit(F&& f, Args&&... args) 
        -> std::future<std::invoke_result_t<F, Args...>> {
        
        using ReturnType = std::invoke_result_t<F, Args...>;
        
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        
        std::future<ReturnType> result = task->get_future();
        
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            
            if (m_stop) {
                throw std::runtime_error("Cannot submit to stopped ThreadPool");
            }
            
            m_tasks.emplace([task]() { (*task)(); });
        }
        
        m_condition.notify_one();
        return result;
    }
    
    /**
     * Submit a task with priority
     * @param priority Task priority (higher = more priority)
     * @param f Function to execute
     * @param args Function arguments
     * @return Future for the result
     */
    template<class F, class... Args>
    auto submitPriority(int priority, F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>> {
        
        using ReturnType = std::invoke_result_t<F, Args...>;
        
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        
        std::future<ReturnType> result = task->get_future();
        
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            
            if (m_stop) {
                throw std::runtime_error("Cannot submit to stopped ThreadPool");
            }
            
            m_priorityTasks.emplace(priority, [task]() { (*task)(); });
        }
        
        m_condition.notify_one();
        return result;
    }
    
    /**
     * Get number of worker threads
     * @return Thread count
     */
    size_t size() const {
        return m_workers.size();
    }
    
    /**
     * Get number of pending tasks
     * @return Queue size
     */
    size_t pendingTasks() const {
        std::unique_lock<std::mutex> lock(m_queueMutex);
        return m_tasks.size() + m_priorityTasks.size();
    }
    
    /**
     * Get number of active jobs
     * @return Active job count
     */
    size_t activeJobs() const {
        return m_activeJobs.load();
    }
    
    /**
     * Check if pool is idle
     * @return true if no tasks running or pending
     */
    bool isIdle() const {
        return pendingTasks() == 0 && activeJobs() == 0;
    }
    
    /**
     * Wait for all tasks to complete
     */
    void waitAll() {
        std::unique_lock<std::mutex> lock(m_queueMutex);
        m_idleCondition.wait(lock, [this] {
            return m_tasks.empty() && m_priorityTasks.empty() && m_activeJobs == 0;
        });
    }
    
    /**
     * Get global thread pool instance
     * @return Reference to global pool
     */
    static ThreadPool& global() {
        static ThreadPool instance;
        return instance;
    }

private:
    /**
     * Worker thread loop
     */
    void workerLoop() {
        while (true) {
            std::function<void()> task;
            
            {
                std::unique_lock<std::mutex> lock(m_queueMutex);
                
                m_condition.wait(lock, [this] {
                    return m_stop || !m_tasks.empty() || !m_priorityTasks.empty();
                });
                
                if (m_stop && m_tasks.empty() && m_priorityTasks.empty()) {
                    return;
                }
                
                // Priority tasks first
                if (!m_priorityTasks.empty()) {
                    task = std::move(m_priorityTasks.top().task);
                    m_priorityTasks.pop();
                } else if (!m_tasks.empty()) {
                    task = std::move(m_tasks.front());
                    m_tasks.pop();
                }
            }
            
            if (task) {
                ++m_activeJobs;
                try {
                    task();
                } catch (...) {
                    // Log error but continue
                }
                --m_activeJobs;
                
                // Notify idle condition
                if (m_tasks.empty() && m_priorityTasks.empty() && m_activeJobs == 0) {
                    m_idleCondition.notify_all();
                }
            }
        }
    }

private:
    struct PriorityTask {
        int priority;
        std::function<void()> task;
        
        PriorityTask(int p, std::function<void()> t)
            : priority(p), task(std::move(t)) {}
        
        bool operator<(const PriorityTask& other) const {
            return priority < other.priority;
        }
    };
    
    std::vector<std::thread> m_workers;
    std::queue<std::function<void()>> m_tasks;
    std::priority_queue<PriorityTask> m_priorityTasks;
    
    mutable std::mutex m_queueMutex;
    std::condition_variable m_condition;
    std::condition_variable m_idleCondition;
    
    std::atomic<bool> m_stop;
    std::atomic<size_t> m_activeJobs;
};

} // namespace nexus::core
