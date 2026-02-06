#pragma once

/**
 * Logger.hpp
 * 
 * Centralized logging system with multiple log levels and sinks.
 * Uses spdlog as the underlying logging library.
 */

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/fmt/ostr.h>

#include <memory>
#include <string>
#include <filesystem>

namespace konami::core {

/**
 * Log level enumeration
 */
enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    Critical,
    Off
};

/**
 * Logger class - Thread-safe singleton logger
 * 
 * Provides formatted logging with multiple output sinks:
 * - Console output with colors
 * - Rotating file output
 */
class Logger {
public:
    /**
     * Get singleton instance
     * @return Reference to Logger instance
     */
    static Logger& instance() {
        static Logger instance;
        return instance;
    }
    
    /**
     * Initialize the logger
     * @param level Minimum log level
     * @param logDir Log file directory (optional)
     */
    void initialize(LogLevel level = LogLevel::Info, 
                   const std::string& logDir = "") {
        try {
            std::vector<spdlog::sink_ptr> sinks;
            
            // Console sink with colors
            auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            consoleSink->set_level(toSpdlogLevel(level));
            consoleSink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");
            sinks.push_back(consoleSink);
            
            // File sink
            std::filesystem::path logPath;
            if (logDir.empty()) {
                logPath = std::filesystem::current_path() / "logs" / "konami.log";
            } else {
                logPath = std::filesystem::path(logDir) / "konami.log";
            }
            
            std::filesystem::create_directories(logPath.parent_path());
            
            auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                logPath.string(),
                1024 * 1024 * 10, // 10 MB
                5,                // 5 rotated files
                true              // Rotate on open
            );
            fileSink->set_level(spdlog::level::trace);
            fileSink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] [%s:%#] %v");
            sinks.push_back(fileSink);
            
            // Create logger
            m_logger = std::make_shared<spdlog::logger>("konami", sinks.begin(), sinks.end());
            m_logger->set_level(toSpdlogLevel(level));
            m_logger->flush_on(spdlog::level::warn);
            
            spdlog::set_default_logger(m_logger);
            spdlog::flush_every(std::chrono::seconds(3));
            
            m_initialized = true;
            
        } catch (const spdlog::spdlog_ex& ex) {
            // Fallback to basic console logging
            m_logger = spdlog::stdout_color_mt("konami_fallback");
            m_logger->error("Logger initialization failed: {}", ex.what());
        }
    }
    
    /**
     * Set log level
     * @param level New log level
     */
    void setLevel(LogLevel level) {
        if (m_logger) {
            m_logger->set_level(toSpdlogLevel(level));
        }
    }
    
    /**
     * Flush all log sinks
     */
    void flush() {
        if (m_logger) {
            m_logger->flush();
        }
    }
    
    /**
     * Log trace message
     */
    template<typename... Args>
    void trace(spdlog::format_string_t<Args...> fmt, Args&&... args) {
        if (m_logger) {
            m_logger->trace(fmt, std::forward<Args>(args)...);
        }
    }
    
    /**
     * Log debug message
     */
    template<typename... Args>
    void debug(spdlog::format_string_t<Args...> fmt, Args&&... args) {
        if (m_logger) {
            m_logger->debug(fmt, std::forward<Args>(args)...);
        }
    }
    
    /**
     * Log info message
     */
    template<typename... Args>
    void info(spdlog::format_string_t<Args...> fmt, Args&&... args) {
        if (m_logger) {
            m_logger->info(fmt, std::forward<Args>(args)...);
        }
    }
    
    /**
     * Log warning message
     */
    template<typename... Args>
    void warn(spdlog::format_string_t<Args...> fmt, Args&&... args) {
        if (m_logger) {
            m_logger->warn(fmt, std::forward<Args>(args)...);
        }
    }
    
    /**
     * Log error message
     */
    template<typename... Args>
    void error(spdlog::format_string_t<Args...> fmt, Args&&... args) {
        if (m_logger) {
            m_logger->error(fmt, std::forward<Args>(args)...);
        }
    }
    
    /**
     * Log critical message
     */
    template<typename... Args>
    void critical(spdlog::format_string_t<Args...> fmt, Args&&... args) {
        if (m_logger) {
            m_logger->critical(fmt, std::forward<Args>(args)...);
        }
    }

private:
    Logger() = default;
    ~Logger() {
        if (m_logger) {
            m_logger->flush();
        }
        spdlog::shutdown();
    }
    
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    /**
     * Convert LogLevel to spdlog::level
     */
    static spdlog::level::level_enum toSpdlogLevel(LogLevel level) {
        switch (level) {
            case LogLevel::Trace:    return spdlog::level::trace;
            case LogLevel::Debug:    return spdlog::level::debug;
            case LogLevel::Info:     return spdlog::level::info;
            case LogLevel::Warn:     return spdlog::level::warn;
            case LogLevel::Error:    return spdlog::level::err;
            case LogLevel::Critical: return spdlog::level::critical;
            case LogLevel::Off:      return spdlog::level::off;
            default:                 return spdlog::level::info;
        }
    }

private:
    std::shared_ptr<spdlog::logger> m_logger;
    bool m_initialized{false};
};

} // namespace konami::core

// Convenience macros
#define LOG_TRACE(...)    konami::core::Logger::instance().trace(__VA_ARGS__)
#define LOG_DEBUG(...)    konami::core::Logger::instance().debug(__VA_ARGS__)
#define LOG_INFO(...)     konami::core::Logger::instance().info(__VA_ARGS__)
#define LOG_WARN(...)     konami::core::Logger::instance().warn(__VA_ARGS__)
#define LOG_ERROR(...)    konami::core::Logger::instance().error(__VA_ARGS__)
#define LOG_CRITICAL(...) konami::core::Logger::instance().critical(__VA_ARGS__)
