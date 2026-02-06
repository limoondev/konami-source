#pragma once

/**
 * EventBus.hpp
 * 
 * Thread-safe event bus for decoupled communication between components.
 * Supports publish/subscribe pattern with type-safe event data.
 */

#include <nlohmann/json.hpp>
#include <functional>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <string>
#include <memory>
#include <atomic>

namespace konami::core {

using json = nlohmann::json;
using EventCallback = std::function<void(const json&)>;

/**
 * Event subscription handle
 */
class Subscription {
public:
    Subscription(uint64_t id, const std::string& event)
        : m_id(id), m_event(event), m_active(true) {}
    
    uint64_t getId() const { return m_id; }
    const std::string& getEvent() const { return m_event; }
    bool isActive() const { return m_active; }
    void cancel() { m_active = false; }

private:
    uint64_t m_id;
    std::string m_event;
    std::atomic<bool> m_active;
};

using SubscriptionPtr = std::shared_ptr<Subscription>;

/**
 * EventBus - Thread-safe publish/subscribe event system
 * 
 * Features:
 * - Named events with JSON payloads
 * - Multiple subscribers per event
 * - Subscription management
 * - Thread-safe operations
 */
class EventBus {
public:
    /**
     * Get singleton instance
     * @return Reference to EventBus instance
     */
    static EventBus& instance() {
        static EventBus instance;
        return instance;
    }
    
    /**
     * Subscribe to an event
     * @param event Event name
     * @param callback Callback function
     * @return Subscription handle for unsubscribing
     */
    SubscriptionPtr subscribe(const std::string& event, EventCallback callback) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        uint64_t id = m_nextId++;
        auto subscription = std::make_shared<Subscription>(id, event);
        
        m_subscribers[event].push_back({id, std::move(callback), subscription});
        
        return subscription;
    }
    
    /**
     * Unsubscribe from an event
     * @param subscription Subscription handle
     */
    void unsubscribe(const SubscriptionPtr& subscription) {
        if (!subscription) return;
        
        subscription->cancel();
        
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto& subscribers = m_subscribers[subscription->getEvent()];
        subscribers.erase(
            std::remove_if(subscribers.begin(), subscribers.end(),
                [id = subscription->getId()](const SubscriberEntry& entry) {
                    return entry.id == id;
                }),
            subscribers.end()
        );
    }
    
    /**
     * Emit an event
     * @param event Event name
     * @param data Event data
     */
    void emit(const std::string& event, const json& data = json::object()) {
        std::vector<EventCallback> callbacks;
        
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            
            auto it = m_subscribers.find(event);
            if (it != m_subscribers.end()) {
                for (const auto& entry : it->second) {
                    if (entry.subscription->isActive()) {
                        callbacks.push_back(entry.callback);
                    }
                }
            }
        }
        
        // Call callbacks outside of lock
        for (const auto& callback : callbacks) {
            try {
                callback(data);
            } catch (const std::exception& e) {
                // Log error but continue
            }
        }
    }
    
    /**
     * Emit an event asynchronously
     * @param event Event name
     * @param data Event data
     */
    void emitAsync(const std::string& event, const json& data = json::object()) {
        std::thread([this, event, data]() {
            emit(event, data);
        }).detach();
    }
    
    /**
     * Subscribe to an event once (auto-unsubscribe after first call)
     * @param event Event name
     * @param callback Callback function
     * @return Subscription handle
     */
    SubscriptionPtr once(const std::string& event, EventCallback callback) {
        auto subscription = std::make_shared<Subscription>(m_nextId++, event);
        
        auto wrappedCallback = [this, subscription, callback = std::move(callback)](const json& data) {
            callback(data);
            unsubscribe(subscription);
        };
        
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_subscribers[event].push_back({subscription->getId(), std::move(wrappedCallback), subscription});
        }
        
        return subscription;
    }
    
    /**
     * Check if event has subscribers
     * @param event Event name
     * @return true if has subscribers
     */
    bool hasSubscribers(const std::string& event) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_subscribers.find(event);
        return it != m_subscribers.end() && !it->second.empty();
    }
    
    /**
     * Get subscriber count for event
     * @param event Event name
     * @return Number of subscribers
     */
    size_t getSubscriberCount(const std::string& event) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_subscribers.find(event);
        return it != m_subscribers.end() ? it->second.size() : 0;
    }
    
    /**
     * Clear all subscribers for an event
     * @param event Event name
     */
    void clearEvent(const std::string& event) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_subscribers.erase(event);
    }
    
    /**
     * Clear all subscribers
     */
    void clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_subscribers.clear();
    }

private:
    EventBus() = default;
    ~EventBus() = default;
    
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

private:
    struct SubscriberEntry {
        uint64_t id;
        EventCallback callback;
        SubscriptionPtr subscription;
    };
    
    mutable std::mutex m_mutex;
    std::unordered_map<std::string, std::vector<SubscriberEntry>> m_subscribers;
    std::atomic<uint64_t> m_nextId{0};
};

} // namespace konami::core
