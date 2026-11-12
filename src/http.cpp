// Hymnal - HTTP through WinHTTP (see http.h)
#include "http.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <random>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif

namespace fs = std::filesystem;

namespace Http {

#ifdef _WIN32
static std::wstring widen(const std::string& s) { return fs::u8path(s).wstring(); }

struct Url { std::wstring host, path; INTERNET_PORT port = 80; bool https = false; };
static bool parse(const std::string& url, Url& u) {
    URL_COMPONENTS c{};
    c.dwStructSize = sizeof c;
    wchar_t host[512], path[4096];
    c.lpszHostName = host; c.dwHostNameLength = 512;
    c.lpszUrlPath = path; c.dwUrlPathLength = 4096;
    wchar_t extra[4096]; c.lpszExtraInfo = extra; c.dwExtraInfoLength = 4096;
    const std::wstring w = widen(url);
    if (!WinHttpCrackUrl(w.c_str(), 0, 0, &c)) return false;
    u.host.assign(host, c.dwHostNameLength);
    u.path.assign(path, c.dwUrlPathLength);
    u.path.append(extra, c.dwExtraInfoLength);
    u.port = c.nPort;
    u.https = c.nScheme == INTERNET_SCHEME_HTTPS;
    return true;
}

// one request; follows redirects (Hugging Face sends its files from a CDN); body read in full, or streamed to sink
static Response request(const std::string& method, const std::string& url, const std::string& contentType, const std::string& body,
                        std::function<bool(const char*, size_t, int64_t)> sink = nullptr, int64_t from = 0) {
    Response r;
    Url u;
    if (!parse(url, u)) return r;
    HINTERNET s = WinHttpOpen(L"Hymnal", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!s) return r;
    // the local server can take a while with big uploads; downloads can be long: generous timeouts
    WinHttpSetTimeouts(s, 10000, 10000, 600000, 600000);
    HINTERNET c = WinHttpConnect(s, u.host.c_str(), u.port, 0);
    HINTERNET q = c ? WinHttpOpenRequest(c, widen(method).c_str(), u.path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, u.https ? WINHTTP_FLAG_SECURE : 0) : nullptr;
    if (q) {
        DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
        WinHttpSetOption(q, WINHTTP_OPTION_REDIRECT_POLICY, &redirect, sizeof redirect);
        std::wstring headers;
        if (!contentType.empty()) headers = L"Content-Type: " + widen(contentType) + L"\r\n";
        if (from > 0) headers += L"Range: bytes=" + std::to_wstring(from) + L"-\r\n";   // carrying on a download
        if (WinHttpSendRequest(q, headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headers.c_str(), (DWORD)-1L,
                               body.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)body.data(), (DWORD)body.size(), (DWORD)body.size(), 0) &&
            WinHttpReceiveResponse(q, nullptr)) {
            DWORD code = 0, len = sizeof code;
            WinHttpQueryHeaders(q, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &code, &len, WINHTTP_NO_HEADER_INDEX);
            r.status = (int)code;
            wchar_t ct[512]; len = sizeof ct;
            if (WinHttpQueryHeaders(q, WINHTTP_QUERY_CONTENT_TYPE, WINHTTP_HEADER_NAME_BY_INDEX, ct, &len, WINHTTP_NO_HEADER_INDEX))
                r.contentType = fs::path(std::wstring(ct, len / sizeof(wchar_t))).u8string();
            int64_t total = -1;
            wchar_t cl[64]; len = sizeof cl;
            if (WinHttpQueryHeaders(q, WINHTTP_QUERY_CONTENT_LENGTH, WINHTTP_HEADER_NAME_BY_INDEX, cl, &len, WINHTTP_NO_HEADER_INDEX)) total = _wtoi64(cl);
            r.length = total;
            std::vector<char> buf(1 << 16);
            for (;;) {
                DWORD got = 0;
                if (!WinHttpReadData(q, buf.data(), (DWORD)buf.size(), &got) || got == 0) break;
                r.received += got;
                if (sink) { if (!sink(buf.data(), got, total)) { r.status = -1; break; } }
                else r.body.append(buf.data(), got);
            }
        }
        WinHttpCloseHandle(q);
    }
    if (c) WinHttpCloseHandle(c);
    WinHttpCloseHandle(s);
    return r;
}
#else
static Response request(const std::string&, const std::string&, const std::string&, const std::string&, std::function<bool(const char*, size_t, int64_t)> = nullptr, int64_t = 0) { return {}; }
#endif

Response get(const std::string& url) { return request("GET", url, "", ""); }
Response postJson(const std::string& url, const std::string& json) { return request("POST", url, "application/json", json); }

Response postMultipart(const std::string& url, const std::vector<Part>& parts) {
    std::mt19937_64 rng(std::random_device{}());
    const std::string boundary = "----HymnalBoundary" + std::to_string(rng());
    std::string body;
    for (const auto& p : parts) {
        body += "--" + boundary + "\r\nContent-Disposition: form-data; name=\"" + p.name + "\"";
        if (!p.filename.empty()) body += "; filename=\"" + p.filename + "\"";
        body += "\r\n";
        if (!p.type.empty()) body += "Content-Type: " + p.type + "\r\n";
        body += "\r\n" + p.data + "\r\n";
    }
    body += "--" + boundary + "--\r\n";
    return request("POST", url, "multipart/form-data; boundary=" + boundary, body);
}

bool download(const std::string& url, const std::string& dest, std::function<void(int64_t, int64_t)> progress, std::atomic<bool>* stop, std::string& err) {
    // into dest.part, carrying on from whatever is already there; moved into place only once every byte has come.
    // A dropped connection is tried again (from where it broke) a few times before giving up.
    std::error_code ec;
    fs::create_directories(fs::u8path(dest).parent_path(), ec);
    const std::string part = dest + ".part";
    for (int attempt = 0; attempt < 8; ++attempt) {
        if (stop && *stop) { err = "Stopped"; return false; }
        int64_t have = fs::exists(fs::u8path(part), ec) ? (int64_t)fs::file_size(fs::u8path(part), ec) : 0;
#ifdef _WIN32
        FILE* f = _wfopen(fs::u8path(part).wstring().c_str(), have > 0 ? L"ab" : L"wb");
#else
        FILE* f = fopen(part.c_str(), have > 0 ? "ab" : "wb");
#endif
        if (!f) { err = "Can't write " + part; return false; }
        int64_t done = have, whole = -1;
        bool restarted = false;
        const Response r = request("GET", url, "", "", [&](const char* d, size_t n, int64_t total) {
            if (stop && *stop) return false;
            if (whole < 0) whole = total < 0 ? -1 : total + done;
            if (fwrite(d, 1, n, f) != n) return false;
            done += (int64_t)n;
            if (progress) progress(done, whole);
            return true;
        }, have);
        // the server ignored the range and sent the whole file: start the part over
        if (r.status == 200 && have > 0) { fclose(f); f = nullptr; fs::remove(fs::u8path(part), ec); restarted = true; }
        if (f) fclose(f);
        if (restarted) continue;
        if (r.status == 416) {   // asked for bytes past the end: the part is already whole
            fs::rename(fs::u8path(part), fs::u8path(dest), ec);
            if (ec) { err = "Can't move it into place: " + ec.message(); return false; }
            return true;
        }
        if (r.status == -1) { err = "Stopped"; return false; }
        if (r.status != 200 && r.status != 206) {
            if (r.status == 0 && attempt < 7) continue;   // no connection: try again
            err = r.status == 0 ? "Couldn't connect" : "HTTP " + std::to_string(r.status);
            return false;
        }
        if (r.length >= 0 && r.received < r.length) continue;   // broke off part-way: carry on from here
        fs::rename(fs::u8path(part), fs::u8path(dest), ec);
        if (ec) { err = "Can't move it into place: " + ec.message(); return false; }
        return true;
    }
    err = "The download kept breaking off: try again later (it carries on from where it stopped)";
    return false;
}

std::vector<MixedPart> splitMixed(const Response& r) {
    std::vector<MixedPart> out;
    const size_t b = r.contentType.find("boundary=");
    if (b == std::string::npos) return out;
    std::string boundary = r.contentType.substr(b + 9);
    if (!boundary.empty() && boundary.front() == '"') boundary = boundary.substr(1, boundary.find('"', 1) - 1);
    const size_t semi = boundary.find(';');
    if (semi != std::string::npos) boundary.resize(semi);
    const std::string delim = "--" + boundary;
    size_t pos = r.body.find(delim);
    while (pos != std::string::npos) {
        pos += delim.size();
        if (r.body.compare(pos, 2, "--") == 0) break;
        if (r.body.compare(pos, 2, "\r\n") == 0) pos += 2;
        const size_t headEnd = r.body.find("\r\n\r\n", pos);
        if (headEnd == std::string::npos) break;
        MixedPart p;
        const std::string head = r.body.substr(pos, headEnd - pos);
        auto header = [&](const char* name) {
            std::string lower = head;
            std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return (char)std::tolower(c); });
            const size_t at = lower.find(name);
            if (at == std::string::npos) return std::string();
            const size_t s = at + strlen(name), e = head.find("\r\n", s);
            std::string v = head.substr(s, e == std::string::npos ? std::string::npos : e - s);
            while (!v.empty() && v.front() == ' ') v.erase(v.begin());
            return v;
        };
        p.type = header("content-type:");
        p.disposition = header("content-disposition:");
        const size_t dataStart = headEnd + 4;
        const size_t next = r.body.find("\r\n" + delim, dataStart);
        if (next == std::string::npos) break;
        p.data = r.body.substr(dataStart, next - dataStart);
        out.push_back(std::move(p));
        pos = next + 2;
    }
    return out;
}

}  // namespace Http
