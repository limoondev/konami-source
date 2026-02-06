#pragma once

/**
 * DownloadTask.hpp
 * 
 * Represents a single download task with metadata.
 */

#include <string>
#include <atomic>

namespace nexus::core::downloader {

/**
 * Download task status
 */
enum class DownloadStatus {
    Pending,
    Downloading,
    Paused,
    Completed,
    Failed,
    Cancelled
};

/**
 * DownloadTask - Single download item
 */
struct DownloadTask {
    // Task ID (assigned by DownloadManager)
    std::string id;
    
    // Source URL
    std::string url;
    
    // Destination file path
    std::string destination;
    
    // Expected SHA1 hash (optional)
    std::string sha1;
    
    // Expected file size (optional, 0 = unknown)
    size_t expectedSize{0};
    
    // Task status
    std::atomic<DownloadStatus> status{DownloadStatus::Pending};
    
    // Error message (if failed)
    std::string error;
    
    // Cancellation flag
    std::atomic<bool> cancelled{false};
    
    // Number of retry attempts made
    int retryAttempts{0};
    
    // User data (optional)
    void* userData{nullptr};
    
    /**
     * Default constructor
     */
    DownloadTask() = default;
    
    /**
     * Constructor with URL and destination
     */
    DownloadTask(const std::string& url_, const std::string& dest_)
        : url(url_), destination(dest_) {}
    
    /**
     * Constructor with URL, destination, and SHA1
     */
    DownloadTask(const std::string& url_, const std::string& dest_, const std::string& sha1_)
        : url(url_), destination(dest_), sha1(sha1_) {}
    
    /**
     * Copy constructor
     */
    DownloadTask(const DownloadTask& other)
        : id(other.id)
        , url(other.url)
        , destination(other.destination)
        , sha1(other.sha1)
        , expectedSize(other.expectedSize)
        , status(other.status.load())
        , error(other.error)
        , cancelled(other.cancelled.load())
        , retryAttempts(other.retryAttempts)
        , userData(other.userData) {}
    
    /**
     * Copy assignment
     */
    DownloadTask& operator=(const DownloadTask& other) {
        if (this != &other) {
            id = other.id;
            url = other.url;
            destination = other.destination;
            sha1 = other.sha1;
            expectedSize = other.expectedSize;
            status = other.status.load();
            error = other.error;
            cancelled = other.cancelled.load();
            retryAttempts = other.retryAttempts;
            userData = other.userData;
        }
        return *this;
    }
    
    /**
     * Check if task is complete
     */
    bool isComplete() const {
        return status == DownloadStatus::Completed ||
               status == DownloadStatus::Failed ||
               status == DownloadStatus::Cancelled;
    }
    
    /**
     * Check if task is successful
     */
    bool isSuccess() const {
        return status == DownloadStatus::Completed;
    }
};

} // namespace nexus::core::downloader
