// Hymnal - HTTP through WinHTTP: the local ACE-Step server (JSON jobs, audio uploads) and model downloads from
// Hugging Face (with progress).
#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace Http {

struct Response {
    int status = 0;                    // 0: couldn't connect
    std::string contentType;
    std::string body;
    int64_t length = -1;               // Content-Length, when the server sent one
    int64_t received = 0;              // bytes that came
};

Response get(const std::string& url);
Response postJson(const std::string& url, const std::string& json);

// multipart/form-data: parts of (name, filename or "", content type, bytes)
struct Part { std::string name, filename, type, data; };
Response postMultipart(const std::string& url, const std::vector<Part>& parts);

// a file downloaded to dest (via dest + ".part"); progress(done, total) as it goes; stop to cancel
bool download(const std::string& url, const std::string& dest, std::function<void(int64_t, int64_t)> progress,
              std::atomic<bool>* stop, std::string& err);

// multipart/mixed: split a response body into its parts (headers + data)
struct MixedPart { std::string type, disposition, data; };
std::vector<MixedPart> splitMixed(const Response& r);

}  // namespace Http
