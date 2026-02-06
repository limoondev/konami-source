// Konami Client - HTTP Client
// Async HTTP client using libcurl

#pragma once

#include <string>
#include <map>
#include <vector>
#include <functional>
#include <memory>
#include <future>
#include <optional>
#include <curl/curl.h>

namespace konami::utils {

/**
 * @brief HTTP response structure
 */
struct HttpResponse {
    int statusCode{0};
    std::string body;
    std::map<std::string, std::string> headers;
    std::string error;
    double downloadTime{0.0};
    int64_t contentLength{0};
    
    bool isSuccess() const {
        return statusCode >= 200 && statusCode < 300;
    }
    
    bool isOk() const { return statusCode == 200; }
    bool isCreated() const { return statusCode == 201; }
    bool isNoContent() const { return statusCode == 204; }
    bool isNotFound() const { return statusCode == 404; }
    bool isUnauthorized() const { return statusCode == 401; }
    bool isForbidden() const { return statusCode == 403; }
    bool isServerError() const { return statusCode >= 500; }
};

/**
 * @brief HTTP request options
 */
struct HttpOptions {
    std::map<std::string, std::string> headers;
    int timeoutSeconds{30};
    int connectTimeoutSeconds{10};
    bool followRedirects{true};
    int maxRedirects{5};
    bool verifySSL{true};
    std::string userAgent{"Konami-Client/1.0"};
    std::string proxyUrl;
    std::string proxyAuth;
    
    // Progress callback
    std::function<void(int64_t downloaded, int64_t total)> progressCallback;
    
    // Custom CA bundle path (optional)
    std::string caBundle;
};

/**
 * @brief Form data for multipart uploads
 */
struct FormData {
    struct Field {
        std::string name;
        std::string value;
        std::string filename;      // For file uploads
        std::string contentType;   // MIME type
        bool isFile{false};
    };
    
    std::vector<Field> fields;
    
    void addField(const std::string& name, const std::string& value) {
        fields.push_back({name, value, "", "", false});
    }
    
    void addFile(const std::string& name, const std::string& filePath,
                 const std::string& contentType = "application/octet-stream") {
        fields.push_back({name, filePath, filePath, contentType, true});
    }
};

/**
 * @brief Async HTTP client with connection pooling
 */
class HttpClient {
public:
    HttpClient();
    ~HttpClient();
    
    // Disable copy
    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;
    
    // Move
    HttpClient(HttpClient&&) noexcept;
    HttpClient& operator=(HttpClient&&) noexcept;
    
    // Singleton instance
    static HttpClient& instance();
    
    // Set default options
    void setDefaultOptions(const HttpOptions& options);
    
    // Synchronous requests
    HttpResponse get(const std::string& url, const HttpOptions& options = {});
    HttpResponse post(const std::string& url, const std::string& body,
                      const HttpOptions& options = {});
    HttpResponse postJson(const std::string& url, const std::string& json,
                          const HttpOptions& options = {});
    HttpResponse postForm(const std::string& url, const FormData& form,
                          const HttpOptions& options = {});
    HttpResponse put(const std::string& url, const std::string& body,
                     const HttpOptions& options = {});
    HttpResponse patch(const std::string& url, const std::string& body,
                       const HttpOptions& options = {});
    HttpResponse del(const std::string& url, const HttpOptions& options = {});
    HttpResponse head(const std::string& url, const HttpOptions& options = {});
    
    // Asynchronous requests
    std::future<HttpResponse> getAsync(const std::string& url,
                                        const HttpOptions& options = {});
    std::future<HttpResponse> postAsync(const std::string& url,
                                         const std::string& body,
                                         const HttpOptions& options = {});
    std::future<HttpResponse> postJsonAsync(const std::string& url,
                                             const std::string& json,
                                             const HttpOptions& options = {});
    
    // Download file
    bool downloadFile(const std::string& url, const std::string& destination,
                      const HttpOptions& options = {});
    std::future<bool> downloadFileAsync(const std::string& url,
                                         const std::string& destination,
                                         const HttpOptions& options = {});
    
    // Upload file
    HttpResponse uploadFile(const std::string& url, const std::string& filePath,
                            const std::string& fieldName = "file",
                            const HttpOptions& options = {});
    
    // URL utilities
    static std::string urlEncode(const std::string& str);
    static std::string urlDecode(const std::string& str);
    static std::string buildQueryString(const std::map<std::string, std::string>& params);
    
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    
    HttpResponse performRequest(const std::string& method, const std::string& url,
                                 const std::string& body, const HttpOptions& options);
    void setupCurl(CURL* curl, const std::string& url, const HttpOptions& options);
    
    static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp);
    static size_t headerCallback(char* buffer, size_t size, size_t nitems, void* userdata);
    static int progressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
                                curl_off_t ultotal, curl_off_t ulnow);
};

/**
 * @brief RAII wrapper for CURL handle
 */
class CurlHandle {
public:
    CurlHandle();
    ~CurlHandle();
    
    CurlHandle(const CurlHandle&) = delete;
    CurlHandle& operator=(const CurlHandle&) = delete;
    
    CurlHandle(CurlHandle&& other) noexcept;
    CurlHandle& operator=(CurlHandle&& other) noexcept;
    
    CURL* get() const { return m_curl; }
    operator CURL*() const { return m_curl; }
    
    void reset();
    
private:
    CURL* m_curl{nullptr};
};

/**
 * @brief Global CURL initialization
 */
class CurlGlobalInit {
public:
    static void init();
    static void cleanup();
    
private:
    static bool s_initialized;
};

} // namespace konami::utils
