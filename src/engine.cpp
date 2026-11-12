// Hymnal - the engine (see engine.h)
#include "engine.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <thread>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace Engine {

static const int PORT = 7891;   // outside 5000-5999 (DREAM Robotics' range) and the usual 8080
static std::string modelsFolder;
#ifdef _WIN32
static HANDLE jobObject = nullptr, process = nullptr;
#endif

static std::string exeDir() {
#ifdef _WIN32
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return fs::path(buf).parent_path().u8string();
#else
    return ".";
#endif
}

std::string url() { return "http://127.0.0.1:" + std::to_string(PORT); }

static std::string appData() {
    const char* a = std::getenv("APPDATA");
    return std::string(a ? a : ".") + "/Atelier";
}

std::string modelsDir() {
    if (!modelsFolder.empty()) return modelsFolder;
    std::error_code ec;
    if (FILE* f = fopen((appData() + "/hymnal/models.txt").c_str(), "rb")) {
        char b[2048] = {0};
        const size_t n = fread(b, 1, sizeof b - 1, f);
        fclose(f);
        std::string s(b, n);
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
        if (!s.empty()) return modelsFolder = s;
    }
#ifdef _WIN32
    if (fs::exists("F:/AIModels", ec)) return modelsFolder = "F:/AIModels/music";
#endif
    return modelsFolder = appData() + "/models/music";
}

void setModelsDir(const std::string& dir) {
    modelsFolder = dir;
    std::error_code ec;
    fs::create_directories(fs::u8path(appData() + "/hymnal"), ec);
    if (FILE* f = fopen((appData() + "/hymnal/models.txt").c_str(), "wb")) { fwrite(dir.data(), 1, dir.size(), f); fclose(f); }
}

std::string logPath() { return appData() + "/hymnal/engine.log"; }

std::string serverExe() {
    std::error_code ec;
#ifdef _WIN32
    const char* name = "ace-server.exe";
#else
    const char* name = "ace-server";
#endif
    const fs::path here = fs::u8path(exeDir());
    for (const fs::path& p : {here / "engine" / name, here / name, fs::path("D:/cc/_src/acestep.cpp/build-cuda") / name,
                              fs::path("D:/cc/_src/acestep.cpp/build") / name, fs::path("D:/cc/_src/acestep.cpp/build/Release") / name})
        if (fs::exists(p, ec)) return p.u8string();
    return "";
}

bool running() {
    const Http::Response r = Http::get(url() + "/health");
    return r.status == 200;
}

bool start(std::string& err) {
    if (running()) return true;
    const std::string exe = serverExe();
    if (exe.empty()) { err = "The ACE-Step engine (ace-server) isn't next to Hymnal"; return false; }
    std::error_code ec;
    fs::create_directories(fs::u8path(modelsDir()), ec);
    fs::create_directories(fs::u8path(logPath()).parent_path(), ec);
#ifdef _WIN32
    if (!jobObject) {   // the engine goes when Hymnal goes, even if Hymnal crashes
        jobObject = CreateJobObjectW(nullptr, nullptr);
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION li{};
        li.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(jobObject, JobObjectExtendedLimitInformation, &li, sizeof li);
    }
    SECURITY_ATTRIBUTES sa{sizeof sa, nullptr, TRUE};
    HANDLE log = CreateFileW(fs::u8path(logPath()).wstring().c_str(), GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    STARTUPINFOW si{}; si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES; si.hStdOutput = log; si.hStdError = log; si.hStdInput = nullptr;
    PROCESS_INFORMATION pi{};
    std::wstring cmd = L"\"" + fs::u8path(exe).wstring() + L"\" --models \"" + fs::u8path(modelsDir()).wstring() + L"\" --port " + std::to_wstring(PORT) + L" --host 127.0.0.1"
        L" --vae-chunk 256";   // the audio decoded in smaller pieces: room to spare on a 6 GB graphics card
    const std::wstring dir = fs::u8path(exe).parent_path().wstring();
    const BOOL ok = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, dir.c_str(), &si, &pi);
    if (log != INVALID_HANDLE_VALUE) CloseHandle(log);
    if (!ok) { err = "Couldn't start ace-server"; return false; }
    AssignProcessToJobObject(jobObject, pi.hProcess);
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);
    if (process) CloseHandle(process);
    process = pi.hProcess;
    // up within ~30 s (it scans the models; loading happens per job)
    for (int i = 0; i < 300; ++i) {
        if (running()) return true;
        DWORD code = STILL_ACTIVE;
        if (GetExitCodeProcess(process, &code) && code != STILL_ACTIVE) { err = "ace-server stopped at start (see " + logPath() + ")"; return false; }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    err = "ace-server didn't answer (see " + logPath() + ")";
    return false;
#else
    err = "Not on this system yet";
    return false;
#endif
}

void stop() {
#ifdef _WIN32
    if (process) { TerminateProcess(process, 0); CloseHandle(process); process = nullptr; }
#endif
}

bool models(Models& m, std::string& err) {
    const Http::Response r = Http::get(url() + "/props");
    if (r.status != 200) { err = "The engine didn't answer"; return false; }
    Json::Value v;
    if (!Json::parse(r.body, v)) { err = "The engine's answer didn't parse"; return false; }
    const Json::Value* ms = v.get("models");
    auto list = [&](const char* k, std::vector<std::string>& out) {
        out.clear();
        if (!ms) return;
        if (const Json::Value* a = ms->get(k)) for (const auto& x : a->a) if (x.type == Json::Value::STR) out.push_back(x.s);
    };
    list("lm", m.lm); list("embedding", m.embedding); list("dit", m.dit); list("vae", m.vae);
    return true;
}

static bool jobId(const Http::Response& r, std::string& id, std::string& err) {
    Json::Value v;
    if (r.status != 200 || !Json::parse(r.body, v)) {
        Json::Value e;
        err = Json::parse(r.body, e) && !e.str("error").empty() ? e.str("error") : r.status == 0 ? "The engine isn't running" : "The engine said HTTP " + std::to_string(r.status);
        return false;
    }
    id = v.str("id");
    if (id.empty()) { err = "No job id from the engine"; return false; }
    return true;
}

bool submit(const char* endpoint, const std::string& json, std::string& id, std::string& err) {
    return jobId(Http::postJson(url() + endpoint, json), id, err);
}

bool submitAudio(const char* endpoint, const std::string& json, const std::string& audio, const std::string& audioName, std::string& id, std::string& err, const std::string& refAudio) {
    std::vector<Http::Part> parts;
    if (!json.empty()) parts.push_back({"request", "request.json", "application/json", json});
    auto type = [](const std::string& n) { return n.size() > 4 && (n.substr(n.size() - 4) == ".mp3" || n.substr(n.size() - 4) == ".MP3") ? "audio/mpeg" : "audio/wav"; };
    parts.push_back({"audio", audioName, type(audioName), audio});
    if (!refAudio.empty()) parts.push_back({"ref_audio", audioName, type(audioName), refAudio});
    return jobId(Http::postMultipart(url() + endpoint, parts), id, err);
}

bool alive() {
#ifdef _WIN32
    return process && WaitForSingleObject(process, 0) == WAIT_TIMEOUT;
#else
    return true;
#endif
}

Status poll(const std::string& id) {
    const Http::Response r = Http::get(url() + "/job?id=" + id);
    Json::Value v;
    if (r.status != 200 || !Json::parse(r.body, v)) return S_UNKNOWN;
    const std::string s = v.str("status");
    return s == "done" ? S_DONE : s == "failed" ? S_FAILED : s == "cancelled" ? S_CANCELLED : s == "running" ? S_RUNNING : s == "queued" || s == "pending" ? S_QUEUED : S_UNKNOWN;
}

Http::Response result(const std::string& id) { return Http::get(url() + "/job?id=" + id + "&result=1"); }

void cancel(const std::string& id) { Http::postJson(url() + "/job?id=" + id + "&cancel=1", ""); }

}  // namespace Engine
