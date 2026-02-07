/**
 * HttpClient.cpp
 * 
 * HTTP client implementation using cpr (which wraps libcurl).
 * Note: This implementation uses cpr instead of raw curl for simplicity.
 */

#include "HttpClient.hpp"

#include <cpr/cpr.h>
#include <fstream>

namespace konami::utils {

// -- CurlGlobalInit --

bool CurlGlobalInit::s_initialized = false;

void CurlGlobalInit::init() {
    if (!s_initialized) {
        curl_global_init(CURL_GLOBAL_ALL);
        s_initialized = true;
    }
}

void CurlGlobalInit::cleanup() {
    if (s_initialized) {
        curl_global_cleanup();
        s_initialized = false;
    }
}

// -- CurlHandle --

CurlHandle::CurlHandle() : m_curl(curl_easy_init()) {}
CurlHandle::~CurlHandle() { if (m_curl) curl_easy_cleanup(m_curl); }
CurlHandle::CurlHandle(CurlHandle&& other) noexcept : m_curl(other.m_curl) { other.m_curl = nullptr; }
CurlHandle& CurlHandle::operator=(CurlHandle&& other) noexcept {
    if (this != &other) { if (m_curl) curl_easy_cleanup(m_curl); m_curl = other.m_curl; other.m_curl = nullptr; }
    return *this;
}
void CurlHandle::reset() { if (m_curl) curl_easy_reset(m_curl); }

// -- HttpClient::Impl --

struct HttpClient::Impl {
    HttpOptions defaultOptions;
};

// -- HttpClient --

HttpClient::HttpClient() : m_impl(std::make_unique<Impl>()) {
    CurlGlobalInit::init();
}

HttpClient::~HttpClient() = default;
HttpClient::HttpClient(HttpClient&&) noexcept = default;
HttpClient& HttpClient::operator=(HttpClient&&) noexcept = default;

HttpClient& HttpClient::instance() {
    static HttpClient inst;
    return inst;
}

void HttpClient::setDefaultOptions(const HttpOptions& options) {
    m_impl->defaultOptions = options;
}

HttpResponse HttpClient::get(const std::string& url, const HttpOptions& options) {
    return performRequest("GET", url, "", options);
}

HttpResponse HttpClient::post(const std::string& url, const std::string& body, const HttpOptions& options) {
    return performRequest("POST", url, body, options);
}

HttpResponse HttpClient::postJson(const std::string& url, const std::string& json, const HttpOptions& options) {
    HttpOptions opts = options;
    opts.headers["Content-Type"] = "application/json";
    return performRequest("POST", url, json, opts);
}

HttpResponse HttpClient::postForm(const std::string& url, const FormData& /*form*/, const HttpOptions& options) {
    // Simplified: would build multipart form
    return performRequest("POST", url, "", options);
}

HttpResponse HttpClient::put(const std::string& url, const std::string& body, const HttpOptions& options) {
    return performRequest("PUT", url, body, options);
}

HttpResponse HttpClient::patch(const std::string& url, const std::string& body, const HttpOptions& options) {
    return performRequest("PATCH", url, body, options);
}

HttpResponse HttpClient::del(const std::string& url, const HttpOptions& options) {
    return performRequest("DELETE", url, "", options);
}

HttpResponse HttpClient::head(const std::string& url, const HttpOptions& options) {
    return performRequest("HEAD", url, "", options);
}

std::future<HttpResponse> HttpClient::getAsync(const std::string& url, const HttpOptions& options) {
    return std::async(std::launch::async, [this, url, options]() { return get(url, options); });
}

std::future<HttpResponse> HttpClient::postAsync(const std::string& url, const std::string& body, const HttpOptions& options) {
    return std::async(std::launch::async, [this, url, body, options]() { return post(url, body, options); });
}

std::future<HttpResponse> HttpClient::postJsonAsync(const std::string& url, const std::string& json, const HttpOptions& options) {
    return std::async(std::launch::async, [this, url, json, options]() { return postJson(url, json, options); });
}

bool HttpClient::downloadFile(const std::string& url, const std::string& destination, const HttpOptions& options) {
    std::ofstream file(destination, std::ios::binary);
    if (!file.is_open()) return false;

    cpr::Response response = cpr::Download(
        file,
        cpr::Url{url},
        cpr::Timeout{options.timeoutSeconds * 1000}
    );

    file.close();
    return response.status_code == 200;
}

std::future<bool> HttpClient::downloadFileAsync(const std::string& url, const std::string& destination, const HttpOptions& options) {
    return std::async(std::launch::async, [this, url, destination, options]() {
        return downloadFile(url, destination, options);
    });
}

HttpResponse HttpClient::uploadFile(const std::string& url, const std::string& filePath,
                                    const std::string& fieldName, const HttpOptions& options) {
    cpr::Response response = cpr::Post(
        cpr::Url{url},
        cpr::Multipart{{fieldName, cpr::File{filePath}}},
        cpr::Timeout{options.timeoutSeconds * 1000}
    );

    HttpResponse result;
    result.statusCode = response.status_code;
    result.body = response.text;
    return result;
}

std::string HttpClient::urlEncode(const std::string& str) {
    CURL* curl = curl_easy_init();
    if (!curl) return str;
    char* output = curl_easy_escape(curl, str.c_str(), static_cast<int>(str.size()));
    std::string result(output);
    curl_free(output);
    curl_easy_cleanup(curl);
    return result;
}

std::string HttpClient::urlDecode(const std::string& str) {
    CURL* curl = curl_easy_init();
    if (!curl) return str;
    int outLen = 0;
    char* output = curl_easy_unescape(curl, str.c_str(), static_cast<int>(str.size()), &outLen);
    std::string result(output, outLen);
    curl_free(output);
    curl_easy_cleanup(curl);
    return result;
}

std::string HttpClient::buildQueryString(const std::map<std::string, std::string>& params) {
    std::string result;
    for (const auto& [key, value] : params) {
        if (!result.empty()) result += "&";
        result += urlEncode(key) + "=" + urlEncode(value);
    }
    return result;
}

HttpResponse HttpClient::performRequest(const std::string& method, const std::string& url,
                                         const std::string& body, const HttpOptions& options) {
    HttpResponse result;

    try {
        cpr::Header headers;
        for (const auto& [key, value] : m_impl->defaultOptions.headers) headers[key] = value;
        for (const auto& [key, value] : options.headers) headers[key] = value;

        std::string ua = options.userAgent.empty() ? m_impl->defaultOptions.userAgent : options.userAgent;
        if (ua.empty()) ua = "Konami-Client/1.0";

        int timeout = options.timeoutSeconds > 0 ? options.timeoutSeconds : m_impl->defaultOptions.timeoutSeconds;
        if (timeout <= 0) timeout = 30;

        cpr::Response response;

        if (method == "GET") {
            response = cpr::Get(cpr::Url{url}, headers, cpr::Timeout{timeout * 1000}, cpr::UserAgent{ua});
        } else if (method == "POST") {
            response = cpr::Post(cpr::Url{url}, headers, cpr::Body{body}, cpr::Timeout{timeout * 1000}, cpr::UserAgent{ua});
        } else if (method == "PUT") {
            response = cpr::Put(cpr::Url{url}, headers, cpr::Body{body}, cpr::Timeout{timeout * 1000}, cpr::UserAgent{ua});
        } else if (method == "PATCH") {
            response = cpr::Patch(cpr::Url{url}, headers, cpr::Body{body}, cpr::Timeout{timeout * 1000}, cpr::UserAgent{ua});
        } else if (method == "DELETE") {
            response = cpr::Delete(cpr::Url{url}, headers, cpr::Timeout{timeout * 1000}, cpr::UserAgent{ua});
        } else if (method == "HEAD") {
            response = cpr::Head(cpr::Url{url}, headers, cpr::Timeout{timeout * 1000}, cpr::UserAgent{ua});
        }

        result.statusCode = response.status_code;
        result.body = response.text;
        result.error = response.error.message;
        result.downloadTime = response.elapsed;

    } catch (const std::exception& e) {
        result.statusCode = 0;
        result.error = e.what();
    }

    return result;
}

void HttpClient::setupCurl(CURL* /*curl*/, const std::string& /*url*/, const HttpOptions& /*options*/) {
    // Using cpr instead of raw curl
}

size_t HttpClient::writeCallback(void* /*contents*/, size_t size, size_t nmemb, void* /*userp*/) {
    return size * nmemb;
}

size_t HttpClient::headerCallback(char* /*buffer*/, size_t size, size_t nitems, void* /*userdata*/) {
    return size * nitems;
}

int HttpClient::progressCallback(void* /*clientp*/, curl_off_t /*dltotal*/, curl_off_t /*dlnow*/,
                                 curl_off_t /*ultotal*/, curl_off_t /*ulnow*/) {
    return 0;
}

} // namespace konami::utils
