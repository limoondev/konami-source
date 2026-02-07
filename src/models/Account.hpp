#pragma once

#include <string>
#include <chrono>

namespace konami::models {

enum class AccountType {
    Microsoft,
    Offline
};

struct Account {
    std::string uuid;
    std::string username;
    AccountType type{AccountType::Microsoft};
    std::string accessToken;
    std::string refreshToken;
    std::string avatarUrl;
    std::chrono::system_clock::time_point tokenExpiry;

    bool isExpired() const {
        return std::chrono::system_clock::now() >= tokenExpiry;
    }
};

} // namespace konami::models
