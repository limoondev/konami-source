#pragma once

/**
 * CacheManager.hpp
 * 
 * Intelligent caching system with LZ4 compression.
 * Manages downloaded files to avoid redundant downloads.
 */

#include <string>
#include <filesystem>
#include <unordered_map>
#include <mutex>
#include <optional>
#include <chrono>

namespace konami::core::downloader {

/**
 * Cache entry metadata
 */
struct CacheEntry {
    std::string hash;
    std::string originalPath;
    size_t size{0};
    bool compressed{false};
    std::chrono::system_clock::time_point lastAccess;
    int accessCount{0};
};

/**
 * CacheManager - File caching with compression
 * 
 * Features:
 * - Content-addressed storage using SHA1
 * - LZ4 compression for space efficiency
 * - LRU eviction policy
 * - Configurable cache size limit
 */
class CacheManager {
public:
    /**
     * Constructor
     */
    CacheManager();
    
    /**
     * Destructor
     */
    ~CacheManager();
    
    /**
     * Initialize cache
     * @param cachePath Cache directory path
     * @param maxSize Maximum cache size in bytes (default: 2GB)
     */
    void initialize(const std::string& cachePath, size_t maxSize = 2ULL * 1024 * 1024 * 1024);
    
    /**
     * Shutdown cache manager
     */
    void shutdown();
    
    /**
     * Add file to cache
     * @param filePath File to cache
     * @param hash Content hash (SHA1)
     * @return true if added successfully
     */
    bool add(const std::string& filePath, const std::string& hash);
    
    /**
     * Check if hash exists in cache
     * @param hash Content hash
     * @return true if cached
     */
    bool has(const std::string& hash) const;
    
    /**
     * Get cached file path
     * @param hash Content hash
     * @return Cached file path if exists
     */
    std::optional<std::string> get(const std::string& hash);
    
    /**
     * Copy cached file to destination
     * @param hash Content hash
     * @param destination Destination path
     * @return true if copied successfully
     */
    bool copyTo(const std::string& hash, const std::string& destination);
    
    /**
     * Remove entry from cache
     * @param hash Content hash
     * @return true if removed
     */
    bool remove(const std::string& hash);
    
    /**
     * Clear entire cache
     */
    void clear();
    
    /**
     * Get current cache size
     * @return Cache size in bytes
     */
    size_t getCurrentSize() const { return m_currentSize; }
    
    /**
     * Get maximum cache size
     * @return Maximum size in bytes
     */
    size_t getMaxSize() const { return m_maxSize; }
    
    /**
     * Set maximum cache size
     * @param maxSize Maximum size in bytes
     */
    void setMaxSize(size_t maxSize);
    
    /**
     * Get cache hit count
     * @return Number of cache hits
     */
    size_t getHitCount() const { return m_hitCount; }
    
    /**
     * Get cache miss count
     * @return Number of cache misses
     */
    size_t getMissCount() const { return m_missCount; }
    
    /**
     * Get cache entry count
     * @return Number of entries
     */
    size_t getEntryCount() const;
    
    /**
     * Run cache maintenance (eviction, cleanup)
     */
    void runMaintenance();

private:
    /**
     * Load cache index from disk
     */
    void loadIndex();
    
    /**
     * Save cache index to disk
     */
    void saveIndex() const;
    
    /**
     * Get cache file path for hash
     * @param hash Content hash
     * @return Cache file path
     */
    std::filesystem::path getCachePath(const std::string& hash) const;
    
    /**
     * Compress file using LZ4
     * @param input Input file path
     * @param output Output file path
     * @return true if compressed
     */
    bool compressFile(const std::string& input, const std::string& output) const;
    
    /**
     * Decompress LZ4 file
     * @param input Input file path
     * @param output Output file path
     * @return true if decompressed
     */
    bool decompressFile(const std::string& input, const std::string& output) const;
    
    /**
     * Evict entries to make space
     * @param requiredSpace Space needed
     */
    void evict(size_t requiredSpace);
    
    /**
     * Get LRU entry hash
     * @return Hash of LRU entry
     */
    std::string getLRUEntry() const;

private:
    std::filesystem::path m_cachePath;
    std::unordered_map<std::string, CacheEntry> m_entries;
    mutable std::mutex m_mutex;
    
    size_t m_maxSize{0};
    size_t m_currentSize{0};
    size_t m_hitCount{0};
    size_t m_missCount{0};
    
    bool m_initialized{false};
    bool m_useCompression{true};
};

} // namespace konami::core::downloader
