#pragma once

/**
 * DownloadManager.hpp
 * 
 * High-performance parallel download manager with caching.
 * Supports multiple concurrent downloads with progress tracking.
 */

#include "DownloadTask.hpp"
#include "CacheManager.hpp"
#include "../ThreadPool.hpp"

#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>
#include <memory>
#include <unordered_map>

namespace nexus::core::downloader {

/**
 * Download progress callback
 */
using DownloadProgressCallback = std::function<void(
    const std::string& taskId,
    size_t bytesDownloaded,
    size_t totalBytes,
    float speed
)>;

/**
 * Download completion callback
 */
using DownloadCompleteCallback = std::function<void(
    const std::string& taskId,
    bool success,
    const std::string& error
)>;

/**
 * Overall progress callback
 */
using OverallProgressCallback = std::function<void(
    size_t completed,
    size_t total,
    size_t bytesDownloaded,
    size_t totalBytes
)>;

/**
 * Download queue entry
 */
struct QueuedDownload {
    DownloadTask task;
    int priority{0};
    DownloadProgressCallback progressCallback;
    DownloadCompleteCallback completeCallback;
    
    bool operator<(const QueuedDownload& other) const {
        return priority < other.priority;
    }
};

/**
 * DownloadManager - Parallel download management
 * 
 * Features:
 * - Up to 10 concurrent downloads
 * - Priority queue for downloads
 * - Automatic retry with exponential backoff
 * - Checksum verification
 * - Progress tracking per task and overall
 * - Bandwidth limiting
 * - Cache integration
 */
class DownloadManager {
public:
    /**
     * Constructor
     */
    DownloadManager();
    
    /**
     * Destructor
     */
    ~DownloadManager();
    
    // Disable copy
    DownloadManager(const DownloadManager&) = delete;
    DownloadManager& operator=(const DownloadManager&) = delete;
    
    /**
     * Initialize the download manager
     */
    void initialize();
    
    /**
     * Shutdown and cancel all downloads
     */
    void shutdown();
    
    /**
     * Add a download task
     * @param task Download task
     * @param priority Priority (higher = more priority)
     * @param progressCallback Progress callback
     * @param completeCallback Completion callback
     * @return Task ID
     */
    std::string addDownload(
        const DownloadTask& task,
        int priority = 0,
        DownloadProgressCallback progressCallback = nullptr,
        DownloadCompleteCallback completeCallback = nullptr
    );
    
    /**
     * Add multiple downloads
     * @param tasks Vector of download tasks
     * @param priority Priority
     * @param progressCallback Progress callback
     * @param completeCallback Completion callback
     * @return Vector of task IDs
     */
    std::vector<std::string> addDownloads(
        const std::vector<DownloadTask>& tasks,
        int priority = 0,
        DownloadProgressCallback progressCallback = nullptr,
        DownloadCompleteCallback completeCallback = nullptr
    );
    
    /**
     * Cancel a download
     * @param taskId Task ID
     * @return true if cancelled
     */
    bool cancelDownload(const std::string& taskId);
    
    /**
     * Cancel all downloads
     */
    void cancelAll();
    
    /**
     * Pause all downloads
     */
    void pauseAll();
    
    /**
     * Resume all downloads
     */
    void resumeAll();
    
    /**
     * Wait for all downloads to complete
     */
    void waitForAll();
    
    /**
     * Get download progress
     * @param taskId Task ID
     * @return Progress (0.0 - 1.0)
     */
    float getProgress(const std::string& taskId) const;
    
    /**
     * Get overall progress
     * @return Overall progress (0.0 - 1.0)
     */
    float getOverallProgress() const;
    
    /**
     * Get number of pending downloads
     * @return Pending count
     */
    size_t getPendingCount() const;
    
    /**
     * Get number of active downloads
     * @return Active count
     */
    size_t getActiveCount() const;
    
    /**
     * Get total download speed (bytes/sec)
     * @return Current download speed
     */
    size_t getCurrentSpeed() const;
    
    /**
     * Set maximum concurrent downloads
     * @param max Maximum concurrent downloads
     */
    void setMaxConcurrent(size_t max);
    
    /**
     * Set bandwidth limit (bytes/sec, 0 = unlimited)
     * @param limit Bandwidth limit
     */
    void setBandwidthLimit(size_t limit);
    
    /**
     * Set overall progress callback
     * @param callback Progress callback
     */
    void setOverallProgressCallback(OverallProgressCallback callback);
    
    /**
     * Get cache manager
     * @return Reference to cache manager
     */
    CacheManager& getCacheManager() { return *m_cacheManager; }

private:
    /**
     * Worker function for download threads
     */
    void workerLoop();
    
    /**
     * Process a download task
     * @param queued Queued download
     */
    void processDownload(QueuedDownload queued);
    
    /**
     * Execute download with retry
     * @param task Download task
     * @param progressCallback Progress callback
     * @return true if successful
     */
    bool executeDownload(
        DownloadTask& task,
        const DownloadProgressCallback& progressCallback
    );
    
    /**
     * Verify file checksum
     * @param task Download task
     * @return true if checksum matches
     */
    bool verifyChecksum(const DownloadTask& task);
    
    /**
     * Generate unique task ID
     * @return Task ID
     */
    std::string generateTaskId();

private:
    std::unique_ptr<CacheManager> m_cacheManager;
    std::unique_ptr<ThreadPool> m_threadPool;
    
    std::priority_queue<QueuedDownload> m_queue;
    std::unordered_map<std::string, QueuedDownload> m_activeTasks;
    std::unordered_map<std::string, float> m_taskProgress;
    
    mutable std::mutex m_mutex;
    std::condition_variable m_condition;
    std::condition_variable m_completionCondition;
    
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_paused{false};
    std::atomic<size_t> m_maxConcurrent{10};
    std::atomic<size_t> m_bandwidthLimit{0};
    std::atomic<size_t> m_currentSpeed{0};
    
    std::atomic<size_t> m_totalTasks{0};
    std::atomic<size_t> m_completedTasks{0};
    std::atomic<size_t> m_totalBytes{0};
    std::atomic<size_t> m_downloadedBytes{0};
    
    OverallProgressCallback m_overallProgressCallback;
    
    std::atomic<uint64_t> m_nextTaskId{0};
    bool m_initialized{false};
};

} // namespace nexus::core::downloader
