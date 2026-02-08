#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <openssl/evp.h>
#include <sstream>
#include <iomanip>

namespace konami::utils {

class HashUtils {
public:
    static std::string sha1File(const std::string& filePath) {
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) return "";

        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        if (!ctx) return "";

        if (EVP_DigestInit_ex(ctx, EVP_sha1(), nullptr) != 1) {
            EVP_MD_CTX_free(ctx);
            return "";
        }

        char buffer[8192];
        while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
            EVP_DigestUpdate(ctx, buffer, static_cast<size_t>(file.gcount()));
        }

        unsigned char hash[EVP_MAX_MD_SIZE];
        unsigned int hashLen = 0;
        EVP_DigestFinal_ex(ctx, hash, &hashLen);
        EVP_MD_CTX_free(ctx);

        std::ostringstream oss;
        for (unsigned int i = 0; i < hashLen; ++i) {
            oss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(hash[i]);
        }
        return oss.str();
    }

    static std::string sha256File(const std::string& filePath) {
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) return "";

        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        if (!ctx) return "";

        if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1) {
            EVP_MD_CTX_free(ctx);
            return "";
        }

        char buffer[8192];
        while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
            EVP_DigestUpdate(ctx, buffer, static_cast<size_t>(file.gcount()));
        }

        unsigned char hash[EVP_MAX_MD_SIZE];
        unsigned int hashLen = 0;
        EVP_DigestFinal_ex(ctx, hash, &hashLen);
        EVP_MD_CTX_free(ctx);

        std::ostringstream oss;
        for (unsigned int i = 0; i < hashLen; ++i) {
            oss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(hash[i]);
        }
        return oss.str();
    }

    static std::string sha1String(const std::string& data) {
        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        if (!ctx) return "";

        unsigned char hash[EVP_MAX_MD_SIZE];
        unsigned int hashLen = 0;

        if (EVP_DigestInit_ex(ctx, EVP_sha1(), nullptr) != 1 ||
            EVP_DigestUpdate(ctx, data.data(), data.size()) != 1 ||
            EVP_DigestFinal_ex(ctx, hash, &hashLen) != 1) {
            EVP_MD_CTX_free(ctx);
            return "";
        }
        EVP_MD_CTX_free(ctx);

        std::ostringstream oss;
        for (unsigned int i = 0; i < hashLen; ++i) {
            oss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(hash[i]);
        }
        return oss.str();
    }

    static std::string sha256String(const std::string& data) {
        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        if (!ctx) return "";

        unsigned char hash[EVP_MAX_MD_SIZE];
        unsigned int hashLen = 0;

        if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1 ||
            EVP_DigestUpdate(ctx, data.data(), data.size()) != 1 ||
            EVP_DigestFinal_ex(ctx, hash, &hashLen) != 1) {
            EVP_MD_CTX_free(ctx);
            return "";
        }
        EVP_MD_CTX_free(ctx);

        std::ostringstream oss;
        for (unsigned int i = 0; i < hashLen; ++i) {
            oss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(hash[i]);
        }
        return oss.str();
    }
};

} // namespace konami::utils
