/**
 * CacheManager.cpp
 * 
 * Intelligent caching system with LZ4 compression.
 */

#include "CacheManager.hpp"
#include "../Logger.hpp"

#include <nlohmann/json.hpp>
#include <lz4.h>
#include <fstream>
#include <algorithm>

namespace konami::core::downloader {

using json = nlohmann::json;

CacheManager::CacheManager() = default;
CacheManager::~CacheManager() {
    shutdown();
}

void CacheManager::initialize(const std::string& cachePath, size_t maxSize) {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_cachePath = cachePath;
    m_maxSize = maxSize;

    std::filesystem::create_directories(m_cachePath);

    loadIndex();

    m_initialized = true;
    Logger::instance().info("CacheManager initialized at {}", cachePath);
}

void CacheManager::shutdown() {
    if (!m_initialized) return;

    std::lock_guard<std::mutex> lock(m_mutex);
    saveIndex();
    m_initialized = false;
}

bool CacheManager::add(const std::string& filePath, const std::string& hash) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized) return false;
    if (hash.empty()) return false;

    auto destPath = getCachePath(hash);

    try {
        std::filesystem::create_directories(destPath.parent_path());

        auto fileSize = std::filesystem::file_size(filePath);

        // Evict if needed
        if (m_currentSize + fileSize > m_maxSize) {
            evict(fileSize);
        }

        std::filesystem::copy_file(filePath, destPath,
            std::filesystem::copy_options::overwrite_existing);

        CacheEntry entry;
        entry.hash = hash;
        entry.originalPath = filePath;
        entry.size = fileSize;
        entry.compressed = false;
        entry.lastAccess = std::chrono::system_clock::now();
        entry.accessCount = 1;

        m_entries[hash] = entry;
        m_currentSize += fileSize;

        saveIndex();
        return true;
    } catch (const std::exception& e) {
        Logger::instance().error("Cache add error: {}", e.what());
        return false;
    }
}

bool CacheManager::has(const std::string& hash) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_entries.find(hash) != m_entries.end();
}

std::optional<std::string> CacheManager::get(const std::string& hash) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_entries.find(hash);
    if (it == m_entries.end()) {
        ++m_missCount;
        return std::nullopt;
    }

    auto path = getCachePath(hash);
    if (!std::filesystem::exists(path)) {
        m_entries.erase(it);
        ++m_missCount;
        return std::nullopt;
    }

    it->second.lastAccess = std::chrono::system_clock::now();
    ++it->second.accessCount;
    ++m_hitCount;

    return path.string();
}

bool CacheManager::copyTo(const std::string& hash, const std::string& destination) {
    auto cached = get(hash);
    if (!cached) return false;

    try {
        std::filesystem::create_directories(
            std::filesystem::path(destination).parent_path());
        std::filesystem::copy_file(*cached, destination,
            std::filesystem::copy_options::overwrite_existing);
        return true;
    } catch (const std::exception& e) {
        Logger::instance().error("Cache copyTo error: {}", e.what());
        return false;
    }
}

bool CacheManager::remove(const std::string& hash) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_entries.find(hash);
    if (it == m_entries.end()) return false;

    auto path = getCachePath(hash);
    std::filesystem::remove(path);
    m_currentSize -= it->second.size;
    m_entries.erase(it);

    saveIndex();
    return true;
}

void CacheManager::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::error_code ec;
    std::filesystem::remove_all(m_cachePath, ec);
    std::filesystem::create_directories(m_cachePath);

    m_entries.clear();
    m_currentSize = 0;
    m_hitCount = 0;
    m_missCount = 0;

    saveIndex();
}

void CacheManager::setMaxSize(size_t maxSize) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_maxSize = maxSize;

    if (m_currentSize > m_maxSize) {
        evict(m_currentSize - m_maxSize);
    }
}

size_t CacheManager::getEntryCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_entries.size();
}

void CacheManager::runMaintenance() {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Remove entries whose files no longer exist
    for (auto it = m_entries.begin(); it != m_entries.end(); ) {
        auto path = getCachePath(it->first);
        if (!std::filesystem::exists(path)) {
            m_currentSize -= it->second.size;
            it = m_entries.erase(it);
        } else {
            ++it;
        }
    }

    saveIndex();
}

void CacheManager::loadIndex() {
    auto indexPath = m_cachePath / "index.json";
    if (!std::filesystem::exists(indexPath)) return;

    try {
        std::ifstream file(indexPath);
        auto j = json::parse(file);

        for (auto& [hash, data] : j.items()) {
            CacheEntry entry;
            entry.hash = hash;
            entry.originalPath = data.value("originalPath", "");
            entry.size = data.value("size", size_t(0));
            entry.compressed = data.value("compressed", false);
            entry.accessCount = data.value("accessCount", 0);
            entry.lastAccess = std::chrono::system_clock::now();

            m_entries[hash] = entry;
            m_currentSize += entry.size;
        }
    } catch (const std::exception& e) {
        Logger::instance().warn("Failed to load cache index: {}", e.what());
    }
}

void CacheManager::saveIndex() const {
    auto indexPath = m_cachePath / "index.json";

    try {
        json j;
        for (const auto& [hash, entry] : m_entries) {
            j[hash] = {
                {"originalPath", entry.originalPath},
                {"size", entry.size},
                {"compressed", entry.compressed},
                {"accessCount", entry.accessCount}
            };
        }

        std::ofstream file(indexPath);
        file << j.dump(2);
    } catch (const std::exception& e) {
        Logger::instance().warn("Failed to save cache index: {}", e.what());
    }
}

std::filesystem::path CacheManager::getCachePath(const std::string& hash) const {
    // Use first 2 chars as directory for distribution
    if (hash.size() >= 2) {
        return m_cachePath / hash.substr(0, 2) / hash;
    }
    return m_cachePath / hash;
}

bool CacheManager::compressFile(const std::string& input, const std::string& output) const {
    try {
        std::ifstream in(input, std::ios::binary);
        std::string data((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());

        int maxCompressedSize = LZ4_compressBound(static_cast<int>(data.size()));
        std::vector<char> compressed(maxCompressedSize);

        int compressedSize = LZ4_compress_default(
            data.data(), compressed.data(),
            static_cast<int>(data.size()), maxCompressedSize);

        if (compressedSize <= 0) return false;

        std::ofstream out(output, std::ios::binary);
        // Write original size first
        int32_t origSize = static_cast<int32_t>(data.size());
        out.write(reinterpret_cast<const char*>(&origSize), sizeof(origSize));
        out.write(compressed.data(), compressedSize);

        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool CacheManager::decompressFile(const std::string& input, const std::string& output) const {
    try {
        std::ifstream in(input, std::ios::binary);

        int32_t origSize = 0;
        in.read(reinterpret_cast<char*>(&origSize), sizeof(origSize));

        std::string compressed((std::istreambuf_iterator<char>(in)),
                               std::istreambuf_iterator<char>());

        std::vector<char> decompressed(origSize);

        int result = LZ4_decompress_safe(
            compressed.data(), decompressed.data(),
            static_cast<int>(compressed.size()), origSize);

        if (result < 0) return false;

        std::ofstream out(output, std::ios::binary);
        out.write(decompressed.data(), result);

        return true;
    } catch (const std::exception&) {
        return false;
    }
}

void CacheManager::evict(size_t requiredSpace) {
    while (m_currentSize + requiredSpace > m_maxSize && !m_entries.empty()) {
        auto lru = getLRUEntry();
        if (lru.empty()) break;

        auto it = m_entries.find(lru);
        if (it != m_entries.end()) {
            auto path = getCachePath(lru);
            std::filesystem::remove(path);
            m_currentSize -= it->second.size;
            m_entries.erase(it);
        }
    }
}

std::string CacheManager::getLRUEntry() const {
    std::string lruHash;
    auto oldest = std::chrono::system_clock::time_point::max();

    for (const auto& [hash, entry] : m_entries) {
        if (entry.lastAccess < oldest) {
            oldest = entry.lastAccess;
            lruHash = hash;
        }
    }

    return lruHash;
}

} // namespace konami::core::downloader
