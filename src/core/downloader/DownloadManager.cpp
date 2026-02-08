/**
 * DownloadManager.cpp
 * 
 * Implementation of the parallel download manager.
 */

#include "DownloadManager.hpp"
#include "../Logger.hpp"
#include "../Config.hpp"
#include "../../utils/HashUtils.hpp"
#include "../../utils/PathUtils.hpp"

#include <cpr/cpr.h>
#include <fstream>
#include <chrono>

namespace konami::core::downloader {

DownloadManager::DownloadManager()
    : m_cacheManager(std::make_unique<CacheManager>()) {
}

DownloadManager::~DownloadManager() {
    shutdown();
}

void DownloadManager::initialize() {
    if (m_initialized) return;
    
    Logger::instance().info("Initializing DownloadManager");
    
    // Load configuration
    auto& config = Config::instance();
    m_maxConcurrent = config.get<int>("downloads.maxConcurrent", 10);
    m_bandwidthLimit = config.get<size_t>("downloads.bandwidthLimit", 0);
    
    // Initialize cache
    auto cachePath = utils::PathUtils::getCachePath();
    m_cacheManager->initialize(cachePath.string());
    
    // Create thread pool
    m_threadPool = std::make_unique<ThreadPool>(m_maxConcurrent);
    
    m_running = true;
    m_initialized = true;
    
    Logger::instance().info("DownloadManager initialized (max concurrent: {})", m_maxConcurrent.load());
}

void DownloadManager::shutdown() {
    if (!m_initialized) return;
    
    Logger::instance().info("Shutting down DownloadManager");
    
    cancelAll();
    
    m_running = false;
    m_condition.notify_all();
    
    m_threadPool.reset();
    m_initialized = false;
}

std::string DownloadManager::addDownload(
    const DownloadTask& task,
    int priority,
    DownloadProgressCallback progressCallback,
    DownloadCompleteCallback completeCallback
) {
    std::string taskId = generateTaskId();
    
    QueuedDownload queued;
    queued.task = task;
    queued.task.id = taskId;
    queued.priority = priority;
    queued.progressCallback = std::move(progressCallback);
    queued.completeCallback = std::move(completeCallback);
    
    // Check cache first
    if (!task.sha1.empty() && m_cacheManager->has(task.sha1)) {
        Logger::instance().debug("Using cached file for: {}", task.url);
        
        // Copy from cache
        if (m_cacheManager->copyTo(task.sha1, task.destination)) {
            if (completeCallback) {
                completeCallback(taskId, true, "");
            }
            return taskId;
        }
    }
    
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push(queued);
        m_taskProgress[taskId] = 0.0f;
        ++m_totalTasks;
    }
    
    // Submit to thread pool
    m_threadPool->submit([this, queued]() mutable {
        processDownload(std::move(queued));
    });
    
    m_condition.notify_one();
    
    Logger::instance().debug("Added download task: {} -> {}", task.url, task.destination);
    
    return taskId;
}

std::vector<std::string> DownloadManager::addDownloads(
    const std::vector<DownloadTask>& tasks,
    int priority,
    DownloadProgressCallback progressCallback,
    DownloadCompleteCallback completeCallback
) {
    std::vector<std::string> taskIds;
    taskIds.reserve(tasks.size());
    
    for (const auto& task : tasks) {
        taskIds.push_back(addDownload(task, priority, progressCallback, completeCallback));
    }
    
    return taskIds;
}

bool DownloadManager::cancelDownload(const std::string& taskId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_activeTasks.find(taskId);
    if (it != m_activeTasks.end()) {
        it->second.task.cancelled = true;
        m_activeTasks.erase(it);
        return true;
    }
    
    return false;
}

void DownloadManager::cancelAll() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Cancel all active tasks
    for (auto& [id, task] : m_activeTasks) {
        task.task.cancelled = true;
    }
    m_activeTasks.clear();
    
    // Clear queue
    while (!m_queue.empty()) {
        m_queue.pop();
    }
    
    m_totalTasks = 0;
    m_completedTasks = 0;
    m_totalBytes = 0;
    m_downloadedBytes = 0;
}

void DownloadManager::pauseAll() {
    m_paused = true;
    Logger::instance().info("Downloads paused");
}

void DownloadManager::resumeAll() {
    m_paused = false;
    m_condition.notify_all();
    Logger::instance().info("Downloads resumed");
}

void DownloadManager::waitForAll() {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_completionCondition.wait(lock, [this] {
        return m_queue.empty() && m_activeTasks.empty();
    });
}

float DownloadManager::getProgress(const std::string& taskId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_taskProgress.find(taskId);
    if (it != m_taskProgress.end()) {
        return it->second;
    }
    
    return 0.0f;
}

float DownloadManager::getOverallProgress() const {
    if (m_totalTasks == 0) return 0.0f;
    
    if (m_totalBytes > 0) {
        return static_cast<float>(m_downloadedBytes) / static_cast<float>(m_totalBytes);
    }
    
    return static_cast<float>(m_completedTasks) / static_cast<float>(m_totalTasks);
}

size_t DownloadManager::getPendingCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_queue.size();
}

size_t DownloadManager::getActiveCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_activeTasks.size();
}

size_t DownloadManager::getCurrentSpeed() const {
    return m_currentSpeed;
}

void DownloadManager::setMaxConcurrent(size_t max) {
    m_maxConcurrent = max;
    Config::instance().set("downloads.maxConcurrent", static_cast<int>(max));
}

void DownloadManager::setBandwidthLimit(size_t limit) {
    m_bandwidthLimit = limit;
    Config::instance().set("downloads.bandwidthLimit", limit);
}

void DownloadManager::setOverallProgressCallback(OverallProgressCallback callback) {
    m_overallProgressCallback = std::move(callback);
}

void DownloadManager::processDownload(QueuedDownload queued) {
    if (!m_running || queued.task.cancelled) {
        return;
    }
    
    // Wait if paused
    while (m_paused && m_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_activeTasks[queued.task.id] = queued;
    }
    
    bool success = executeDownload(queued.task, queued.progressCallback);
    
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_activeTasks.erase(queued.task.id);
        m_taskProgress[queued.task.id] = success ? 1.0f : -1.0f;
        ++m_completedTasks;
    }
    
    if (queued.completeCallback) {
        queued.completeCallback(
            queued.task.id,
            success,
            success ? "" : queued.task.error
        );
    }
    
    // Update overall progress
    if (m_overallProgressCallback) {
        m_overallProgressCallback(
            m_completedTasks,
            m_totalTasks,
            m_downloadedBytes,
            m_totalBytes
        );
    }
    
    // Notify completion if all done
    if (m_completedTasks >= m_totalTasks) {
        m_completionCondition.notify_all();
    }
}

bool DownloadManager::executeDownload(
    DownloadTask& task,
    const DownloadProgressCallback& progressCallback
) {
    auto& config = Config::instance();
    int retryCount = config.get<int>("downloads.retryCount", 3);
    int retryDelay = config.get<int>("downloads.retryDelay", 1000);
    int timeout = config.get<int>("downloads.timeout", 30000);
    
    for (int attempt = 0; attempt <= retryCount; ++attempt) {
        if (task.cancelled) {
            return false;
        }
        
        if (attempt > 0) {
            Logger::instance().debug("Retry {} for {}", attempt, task.url);
            std::this_thread::sleep_for(std::chrono::milliseconds(retryDelay * attempt));
        }
        
        try {
            // Create parent directory
            std::filesystem::create_directories(
                std::filesystem::path(task.destination).parent_path()
            );
            
            // Open output file
            std::ofstream file(task.destination, std::ios::binary);
            if (!file.is_open()) {
                task.error = "Failed to open output file";
                continue;
            }
            
            size_t bytesWritten = 0;
            auto startTime = std::chrono::steady_clock::now();
            
            // Download with progress
            cpr::Response response = cpr::Download(
                file,
                cpr::Url{task.url},
                cpr::Timeout{timeout},
                cpr::ProgressCallback([&](cpr::cpr_off_t downloadTotal, cpr::cpr_off_t downloadNow,
                                         cpr::cpr_off_t uploadTotal, cpr::cpr_off_t uploadNow,
                                         intptr_t userdata) -> bool {
                    if (task.cancelled) {
                        return false; // Cancel download
                    }
                    
                    // Calculate speed
                    auto now = std::chrono::steady_clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
                    float speed = elapsed > 0 ? (downloadNow * 1000.0f / elapsed) : 0.0f;
                    
                    m_currentSpeed = static_cast<size_t>(speed);
                    
                    // Update task progress
                    {
                        std::lock_guard<std::mutex> lock(m_mutex);
                        m_taskProgress[task.id] = downloadTotal > 0 
                            ? static_cast<float>(downloadNow) / static_cast<float>(downloadTotal)
                            : 0.0f;
                    }
                    
                    // Callback
                    if (progressCallback) {
                        progressCallback(
                            task.id,
                            static_cast<size_t>(downloadNow),
                            static_cast<size_t>(downloadTotal),
                            speed
                        );
                    }
                    
                    // Bandwidth limiting
                    if (m_bandwidthLimit > 0 && speed > m_bandwidthLimit) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    }
                    
                    return true;
                })
            );
            
            file.close();
            
            if (response.status_code != 200) {
                task.error = "HTTP " + std::to_string(response.status_code);
                std::filesystem::remove(task.destination);
                continue;
            }
            
            // Verify checksum
            if (!task.sha1.empty()) {
                if (!verifyChecksum(task)) {
                    task.error = "Checksum mismatch";
                    std::filesystem::remove(task.destination);
                    continue;
                }
                
                // Add to cache
                m_cacheManager->add(task.destination, task.sha1);
            }
            
            m_downloadedBytes += std::filesystem::file_size(task.destination);
            
            Logger::instance().debug("Downloaded: {}", task.destination);
            return true;
            
        } catch (const std::exception& e) {
            task.error = e.what();
            Logger::instance().warn("Download error: {} - {}", task.url, e.what());
        }
    }
    
    Logger::instance().error("Download failed after {} retries: {}", retryCount, task.url);
    return false;
}

bool DownloadManager::verifyChecksum(const DownloadTask& task) {
    if (task.sha1.empty()) {
        return true;
    }
    
    try {
        std::string fileHash = utils::HashUtils::sha1File(task.destination);
        return fileHash == task.sha1;
    } catch (const std::exception& e) {
        Logger::instance().error("Checksum verification error: {}", e.what());
        return false;
    }
}

std::string DownloadManager::generateTaskId() {
    return "dl_" + std::to_string(++m_nextTaskId);
}

} // namespace konami::core::downloader
