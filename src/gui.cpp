// Hymnal - whole songs, with vocals, from a style and your lyrics, on your own PC. Part of Atelier.
// ACE-Step 1.5 through acestep.cpp (C++, ggml): the songwriter LM writes lyrics and structure, the song model sings
// and plays it. Also: covers and remixes, repainting a section, splitting a song into stems, and a song's key and
// tempo. Every setting is a slider.
//
//   Hymnal [audio file]
//   Hymnal --capture out.bmp [--tab N]      a screenshot from a hidden window, then quit
//   Hymnal --song "style" [--seconds N]      make a song straight away (the songwriter writes the words)
#include "audio.h"
#include "catalog.h"
#include "engine.h"
#include "splash.h"

#include <SDL.h>
#include <SDL_opengl.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl2.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <deque>
#include <filesystem>
#include <functional>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")
#endif

namespace fs = std::filesystem;

#ifndef HYMNAL_VERSION
#define HYMNAL_VERSION "1.0.0"
#endif

// ---------- the Atelier look: greys, the Cursed pink; solid, no gradients, no shadows ----------
static const ImU32 C_BG = IM_COL32(0x1a, 0x1a, 0x1a, 255), C_PANEL = IM_COL32(0x24, 0x24, 0x24, 255),
                   C_LINE = IM_COL32(0x3a, 0x3a, 0x3a, 255), C_TEXT = IM_COL32(0xe6, 0xe6, 0xe6, 255),
                   C_DIM = IM_COL32(0x9a, 0x9a, 0x9a, 255), C_PINK = IM_COL32(0xe0, 0x40, 0x8a, 255),
                   C_DEEP = IM_COL32(0xc2, 0x18, 0x6b, 255), C_WELL = IM_COL32(0x14, 0x14, 0x14, 255);
static ImVec4 col4(ImU32 c) { return ImGui::ColorConvertU32ToFloat4(c); }

static void applyStyle() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 0; s.FrameRounding = 2; s.GrabRounding = 2; s.PopupRounding = 2; s.ScrollbarRounding = 2; s.TabRounding = 2;
    s.WindowBorderSize = 0; s.FrameBorderSize = 0; s.PopupBorderSize = 1;
    s.WindowPadding = ImVec2(12, 10); s.FramePadding = ImVec2(7, 4); s.ItemSpacing = ImVec2(6, 7);
    ImVec4* c = s.Colors;
    c[ImGuiCol_Text] = col4(C_TEXT); c[ImGuiCol_TextDisabled] = col4(C_DIM);
    c[ImGuiCol_WindowBg] = col4(C_PANEL); c[ImGuiCol_ChildBg] = col4(C_PANEL); c[ImGuiCol_PopupBg] = col4(IM_COL32(0x21, 0x21, 0x21, 255));
    c[ImGuiCol_Border] = col4(C_LINE); c[ImGuiCol_Separator] = col4(C_LINE);
    c[ImGuiCol_FrameBg] = col4(C_WELL); c[ImGuiCol_FrameBgHovered] = col4(IM_COL32(0x2e, 0x2e, 0x2e, 255)); c[ImGuiCol_FrameBgActive] = col4(IM_COL32(0x33, 0x33, 0x33, 255));
    c[ImGuiCol_MenuBarBg] = col4(C_BG);
    c[ImGuiCol_CheckMark] = col4(C_PINK); c[ImGuiCol_SliderGrab] = col4(C_PINK); c[ImGuiCol_SliderGrabActive] = col4(C_DEEP);
    c[ImGuiCol_Button] = col4(IM_COL32(0x33, 0x33, 0x33, 255)); c[ImGuiCol_ButtonHovered] = col4(C_DEEP); c[ImGuiCol_ButtonActive] = col4(C_PINK);
    c[ImGuiCol_Header] = col4(IM_COL32(0x33, 0x33, 0x33, 255)); c[ImGuiCol_HeaderHovered] = col4(C_DEEP); c[ImGuiCol_HeaderActive] = col4(C_PINK);
    c[ImGuiCol_Tab] = col4(C_BG); c[ImGuiCol_TabActive] = col4(C_DEEP); c[ImGuiCol_TabHovered] = col4(C_PINK);
    c[ImGuiCol_TabUnfocusedActive] = col4(C_DEEP);
    c[ImGuiCol_PlotHistogram] = col4(C_PINK);
}

static bool fileExists(const char* f) { if (FILE* t = fopen(f, "rb")) { fclose(t); return true; } return false; }

static void loadFont() {
    ImGuiIO& io = ImGui::GetIO();
    const char* f = "C:/Windows/Fonts/segoeui.ttf";
    if (!fileExists(f)) { io.Fonts->AddFontDefault(); return; }
    static const ImWchar ranges[] = {0x0020, 0x00FF, 0x2190, 0x21FF, 0x2600, 0x26FF, 0};
    auto add = [&](const char* file, float size) {
        ImFont* font = io.Fonts->AddFontFromFileTTF(file, size, nullptr, ranges);
        if (fileExists("C:/Windows/Fonts/seguisym.ttf")) {
            ImFontConfig merge; merge.MergeMode = true;
            static const ImWchar sym[] = {0x2190, 0x21FF, 0x2600, 0x26FF, 0};
            io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/seguisym.ttf", size, &merge, sym);
        }
        return font;
    };
    add(f, 16.0f);
    const char* light = fileExists("C:/Windows/Fonts/segoeuil.ttf") ? "C:/Windows/Fonts/segoeuil.ttf" : f;
    Splash::setFonts(add(light, 50.0f), add(f, 13.0f));
}

static std::string fileDialog(bool save, const char* filter, const char* defExt, const std::string& start) {
#ifdef _WIN32
    std::wstring f;
    for (const char* p = filter; *p; ++p) f += (*p == '|') ? L'\0' : (wchar_t)*p;
    f += L'\0'; f += L'\0';
    std::vector<wchar_t> buf(4096, 0);
    if (!start.empty()) { std::wstring s = fs::u8path(start).wstring(); wcsncpy(buf.data(), s.c_str(), buf.size() - 1); }
    OPENFILENAMEW o{};
    o.lStructSize = sizeof(o);
    o.lpstrFilter = f.c_str(); o.lpstrFile = buf.data(); o.nMaxFile = (DWORD)buf.size();
    std::wstring ext = defExt ? std::wstring(defExt, defExt + strlen(defExt)) : L"";
    o.lpstrDefExt = defExt ? ext.c_str() : nullptr;
    o.Flags = save ? (OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR) : (OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR);
    if (!(save ? GetSaveFileNameW(&o) : GetOpenFileNameW(&o))) return "";
    return fs::path(buf.data()).u8string();
#else
    (void)save; (void)filter; (void)defExt; (void)start;
    return "";
#endif
}
static const char* AUDIO = "Audio|*.wav;*.mp3;*.flac;*.ogg;*.m4a;*.aac|All files|*.*|";

static bool readBytes(const std::string& path, std::string& out) {
#ifdef _WIN32
    FILE* f = _wfopen(fs::u8path(path).wstring().c_str(), L"rb");
#else
    FILE* f = fopen(path.c_str(), "rb");
#endif
    if (!f) return false;
    fseek(f, 0, SEEK_END); const long n = ftell(f); fseek(f, 0, SEEK_SET);
    out.resize(n > 0 ? (size_t)n : 0);
    const bool ok = n <= 0 || fread(out.data(), 1, out.size(), f) == out.size();
    fclose(f);
    return ok;
}

// a folder Hymnal can really write to: Music\Hymnal, unless Windows guards Music (Controlled folder access) - then
// Hymnal in the user's own folder. Tried once, by writing a file.
static bool canWrite(const fs::path& d) {
    std::error_code ec;
    fs::create_directories(d, ec);
    const fs::path t = d / ".hymnal-write-test";
    FILE* f = _wfopen(t.wstring().c_str(), L"wb");
    if (!f) return false;
    fclose(f); fs::remove(t, ec);
    return true;
}
static std::string songsDir() {
    static std::string dir;
    if (!dir.empty()) return dir;
#ifdef _WIN32
    PWSTR p = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Music, 0, nullptr, &p))) {
        const fs::path m = fs::path(p) / "Hymnal"; CoTaskMemFree(p);
        if (canWrite(m)) return dir = m.u8string();
    }
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Profile, 0, nullptr, &p))) {
        const fs::path m = fs::path(p) / "Hymnal"; CoTaskMemFree(p);
        if (canWrite(m)) return dir = m.u8string();
    }
#endif
    return dir = "Hymnal";
}

// ---------- the settings: everything is a slider (or a choice) ----------
enum LyricsMode { LY_MINE, LY_WRITE, LY_INSTRUMENTAL };
struct Settings {
    char caption[2048] = "dark synthwave, female vocals, driving bass, 80s drums, cinematic";
    char lyrics[8192] = "[Verse]\nNeon on the water, the city never sleeps\n\n[Chorus]\nSing it to the night\n";
    int lyricsMode = LY_MINE;
    int duration = 0;            // seconds, 0: let it choose
    int bpm = 0;                 // 0: let it choose
    int key = 0;                 // 0: let it choose, then 24 keys
    int timeSig = 0;             // 0 auto, 2, 3, 4, 6
    int language = 0;            // 0 auto
    int variations = 1;
    long long seed = -1;
    float temperature = 0.85f, lmCfg = 2.0f, topP = 0.9f;
    int topK = 0;
    char negative[512] = "";
    bool enrichCaption = true;
    int steps = 0;               // 0: the model's own (turbo 8, sft/base 50)
    float guidance = 0.0f;       // 0: auto
    float shift = 0.0f;          // 0: auto
    int solver = 0;
    int lmModel = -1, songModel = -1;   // -1: the engine's choice
    float peakClip = 10;
};
static Settings st;

static const char* KEYS[] = {"key: let it choose", "C major", "C minor", "C# major", "C# minor", "D major", "D minor", "Eb major", "Eb minor",
                             "E major", "E minor", "F major", "F minor", "F# major", "F# minor", "G major", "G minor", "Ab major",
                             "Ab minor", "A major", "A minor", "Bb major", "Bb minor", "B major", "B minor"};
static const char* LANGS[][2] = {{"language: let it choose", ""}, {"English", "en"}, {"Afrikaans", "af"}, {"Japanese", "ja"}, {"Chinese", "zh"},
                                 {"Korean", "ko"}, {"Spanish", "es"}, {"French", "fr"}, {"German", "de"}, {"Russian", "ru"},
                                 {"Portuguese", "pt"}, {"Italian", "it"}, {"Dutch", "nl"}, {"No words (humming)", "unknown"}};
static const char* SOLVERS[] = {"euler", "sde", "dpm3m", "stork4"};
static const char* SOLVER_NAMES[] = {"solver: euler (steady)", "solver: sde (livelier)", "solver: dpm3m", "solver: stork4"};

// ---------- songs ----------
struct Song {
    std::string title, path, caption, lyrics, key, timeSig, language, kind;
    double bpm = 0, seconds = 0;
    Audio::Clip clip;
    std::vector<float> lo, hi;
    bool analysed = false;
    Audio::Analysis measured;
};
static std::vector<Song> songs;
static int current = -1;

// ---------- the worker ----------
static std::mutex mtx;
static std::deque<std::function<void()>> jobs;
static std::vector<std::function<void()>> onMain;
static std::thread worker;
static std::atomic<bool> busy{false}, workerQuit{false}, stopJob{false};
static std::atomic<float> progress{-1.0f};
static std::string status = "Starting the engine...";
static std::string runningJob;   // the engine's job id, for Stop
static double jobStarted = 0;

static void say(const std::string& s) { std::lock_guard<std::mutex> g(mtx); status = s; }
static void post(std::function<void()> f) { std::lock_guard<std::mutex> g(mtx); onMain.push_back(std::move(f)); }
static void queue(std::function<void()> f) { std::lock_guard<std::mutex> g(mtx); jobs.push_back(std::move(f)); }

static void workerLoop() {
    while (!workerQuit) {
        std::function<void()> job;
        { std::lock_guard<std::mutex> g(mtx); if (!jobs.empty()) { job = std::move(jobs.front()); jobs.pop_front(); } }
        if (!job) { std::this_thread::sleep_for(std::chrono::milliseconds(20)); continue; }
        busy = true; stopJob = false;
        job();
        busy = false; progress = -1.0f;
    }
}

static double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

// wait for an engine job; "what" shows in the status with the time it's taken
static bool waitJob(const std::string& id, const std::string& what, std::string& err) {
    { std::lock_guard<std::mutex> g(mtx); runningJob = id; }
    const double t0 = now();
    for (;;) {
        if (stopJob) { Engine::cancel(id); err = "Stopped"; return false; }
        const Engine::Status s = Engine::poll(id);
        if (s == Engine::S_DONE) break;
        if (s == Engine::S_FAILED) { err = what + " failed (the engine's log: " + Engine::logPath() + ")"; return false; }
        if (s == Engine::S_CANCELLED) { err = "Stopped"; return false; }
        if (s == Engine::S_UNKNOWN && !Engine::alive()) {   // it crashed: say so instead of waiting for ever
            { std::lock_guard<std::mutex> g(mtx); runningJob.clear(); }
            err = "The engine stopped (it may have run out of graphics memory: close other apps using the GPU). Try again: it restarts by itself.";
            return false;
        }
        char m[200]; snprintf(m, sizeof m, "%s... %.0f s", what.c_str(), now() - t0);
        say(m);
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
    }
    { std::lock_guard<std::mutex> g(mtx); runningJob.clear(); }
    return true;
}

static std::string stamp() {
    std::time_t t = std::time(nullptr);
    char b[32]; std::strftime(b, sizeof b, "%Y-%m-%d %H%M%S", std::localtime(&t));
    return b;
}
static std::string safeName(std::string s) {
    for (auto& c : s) if (strchr("<>:\"/\\|?*\n\r\t", c)) c = ' ';
    if (s.size() > 48) s.resize(48);
    while (!s.empty() && (s.back() == ' ' || s.back() == '.')) s.pop_back();
    return s.empty() ? "song" : s;
}

// the synth's answer: one audio part per song (WAV), each saved into Music\Hymnal
static void collect(const Http::Response& r, const std::vector<Json::Value>& meta, const std::string& kind, const std::string& baseTitle) {
    const auto parts = Http::splitMixed(r);
    int n = 0;
    for (const auto& p : parts) {
        if (p.type.find("audio") == std::string::npos) continue;
        Song s;
        std::string err;
        if (!Audio::readWav(p.data, s.clip, err)) continue;
        const Json::Value* m = n < (int)meta.size() ? &meta[n] : (meta.empty() ? nullptr : &meta.back());
        if (m) {
            s.caption = m->str("caption"); s.lyrics = m->str("lyrics"); s.key = m->str("keyscale");
            s.timeSig = m->str("timesignature"); s.language = m->str("vocal_language"); s.bpm = m->num("bpm");
        }
        s.kind = kind;
        s.seconds = s.clip.seconds();
        s.title = baseTitle.empty() ? safeName(s.caption.substr(0, s.caption.find(','))) : baseTitle;
        if (parts.size() > 2) s.title += " (" + std::to_string(n + 1) + ")";
        std::error_code ec;
        fs::create_directories(fs::u8path(songsDir()), ec);
        s.path = (fs::u8path(songsDir()) / fs::u8path(safeName(s.title) + " " + stamp() + (n ? "-" + std::to_string(n) : "") + ".wav")).u8string();
        if (!Audio::saveWav(s.path, s.clip, 16, err)) say("Couldn't save the song: " + err + " (" + s.path + ")");
        Audio::overview(s.clip, 600, s.lo, s.hi);
        ++n;
        post([s] { songs.insert(songs.begin(), s); current = 0; });
    }
}

static std::string modelName(const std::vector<std::string>& list, int i) { return i >= 0 && i < (int)list.size() ? list[i] : ""; }
static Engine::Models engineModels;

static Json::Writer baseRequest(const Settings& s) {
    Json::Writer w;
    w.s("caption", s.caption);
    if (s.lyricsMode == LY_INSTRUMENTAL) w.s("lyrics", "[Instrumental]");
    else if (s.lyricsMode == LY_WRITE) w.s("lyrics", "");
    else w.s("lyrics", s.lyrics);
    if (s.bpm > 0) w.i("bpm", s.bpm);
    if (s.duration > 0) w.f("duration", s.duration);
    if (s.key > 0) w.s("keyscale", KEYS[s.key]);
    if (s.timeSig > 0) w.s("timesignature", std::to_string(s.timeSig));
    if (s.language > 0) w.s("vocal_language", LANGS[s.language][1]);
    w.i("seed", s.seed);
    w.f("lm_temperature", s.temperature).f("lm_cfg_scale", s.lmCfg).f("lm_top_p", s.topP).i("lm_top_k", s.topK);
    if (s.negative[0]) w.s("lm_negative_prompt", s.negative);
    w.b("use_cot_caption", s.enrichCaption);
    w.i("inference_steps", s.steps).f("guidance_scale", s.guidance).f("shift", s.shift).s("solver", SOLVERS[s.solver]);
    w.f("peak_clip", s.peakClip).s("output_format", "wav16");
    const std::string lm = modelName(engineModels.lm, s.lmModel), dit = modelName(engineModels.dit, s.songModel);
    if (!lm.empty()) w.s("lm_model", lm);
    if (!dit.empty()) w.s("synth_model", dit);
    return w;
}

// the engine, started if it isn't; with no song model or audio decoder yet it can't start: point at the Models tab
static bool haveModel(const char* file);
static bool ensureEngine() {
    bool song = false, vae = false, lm = false, text = false;
    for (const auto& m : Catalog::models()) {
        if (!haveModel(m.file)) continue;
        if (m.role == Catalog::SONG) song = true; else if (m.role == Catalog::VAE) vae = true;
        else if (m.role == Catalog::LM) lm = true; else text = true;
    }
    if (!song || !vae || !text) { say("Get the starter models first (Models tab): the engine needs a song model, the text encoder and the audio decoder"); return false; }
    std::string err;
    if (!Engine::start(err)) { say(err); return false; }
    (void)lm;
    return true;
}

static void createSong() {
    const Settings s = st;
    queue([s] {
        std::string err;
        if (!ensureEngine()) return;
        // 1: the songwriter (lyrics, structure, the song's codes)
        Json::Writer w = baseRequest(s);
        w.i("lm_batch_size", s.variations);
        std::string id;
        if (!Engine::submit("/lm", w.str(), id, err)) { say(err); return; }
        if (!waitJob(id, "Writing the song", err)) { say(err); return; }
        const Http::Response r = Engine::result(id);
        Json::Value arr;
        if (!Json::parse(r.body, arr)) { say("The songwriter's answer didn't parse"); return; }
        std::vector<Json::Value> metas = arr.type == Json::Value::ARR ? arr.a : std::vector<Json::Value>{arr};
        // 2: the song model sings and plays each one
        int done = 0;
        for (auto& m : metas) {
            // the songwriter's request, with the song model's settings on top
            std::string body = r.body;
            Json::Writer sw;
            for (const auto& kv : m.o) {
                if (kv.second.type == Json::Value::STR) sw.s(kv.first.c_str(), kv.second.s);
                else if (kv.second.type == Json::Value::NUM) sw.f(kv.first.c_str(), kv.second.n);
                else if (kv.second.type == Json::Value::BOOL) sw.b(kv.first.c_str(), kv.second.b);
            }
            // (later keys win in the engine's reader; these are ours)
            sw.i("inference_steps", s.steps).f("guidance_scale", s.guidance).f("shift", s.shift).s("solver", SOLVERS[s.solver]);
            sw.s("output_format", "wav16").f("peak_clip", s.peakClip);
            if (s.duration > 0) sw.f("duration", s.duration);   // the length slider wins over the songwriter's idea
            const std::string dit = modelName(engineModels.dit, s.songModel);
            if (!dit.empty()) sw.s("synth_model", dit);
            std::string sid;
            if (!Engine::submit("/synth", sw.str(), sid, err)) { say(err); return; }
            char what[64]; snprintf(what, sizeof what, "Singing it%s", metas.size() > 1 ? (" (" + std::to_string(done + 1) + " of " + std::to_string(metas.size()) + ")").c_str() : "");
            if (!waitJob(sid, what, err)) { say(err); return; }
            collect(Engine::result(sid), {m}, "song", "");
            ++done;
        }
        say("Done: " + std::to_string(done) + " song" + (done == 1 ? "" : "s") + " in " + songsDir());
    });
}

// ---------- remix, stems, analysis: they start from a song you drop in ----------
static std::string sourcePath;
static Audio::Clip sourceClip;
static std::vector<float> sourceLo, sourceHi;
static Audio::Analysis sourceMeasured;
static bool sourceMeasuredOk = false;
static std::string understood;   // what ACE-Step heard

static void loadSource(const std::string& path) {
    sourcePath = path;
    sourceMeasuredOk = false; understood.clear();
    queue([path] {
        Audio::Clip c; std::string err;
        say("Opening " + fs::u8path(path).filename().u8string() + "...");
        if (!Audio::load(path, c, err)) { say(err); return; }
        std::vector<float> lo, hi;
        Audio::overview(c, 600, lo, hi);
        const Audio::Analysis a = Audio::analyse(c);
        post([c, lo, hi, a] { sourceClip = c; sourceLo = lo; sourceHi = hi; sourceMeasured = a; sourceMeasuredOk = true; });
        char m[200]; snprintf(m, sizeof m, "%s: %.0f s, about %.0f BPM, %s", fs::u8path(path).filename().u8string().c_str(), c.seconds(), a.bpm, a.key.c_str());
        say(m);
    });
}

// the source as the engine wants it: the file's own bytes (WAV or MP3), else a WAV of what we decoded
static bool sourceBytes(std::string& bytes, std::string& name) {
    std::string ext = fs::u8path(sourcePath).extension().u8string();
    for (auto& c : ext) c = (char)std::tolower((unsigned char)c);
    if ((ext == ".wav" || ext == ".mp3") && readBytes(sourcePath, bytes)) { name = fs::u8path(sourcePath).filename().u8string(); return true; }
    if (sourceClip.empty()) return false;
    bytes = Audio::wavBytes(sourceClip, 16);
    name = fs::u8path(sourcePath).stem().u8string() + ".wav";
    return true;
}

enum RemixMode { RM_COVER, RM_CLOSE, RM_REPAINT, RM_COMPLETE, RM_LEGO, RM_COUNT };
static int remixMode = RM_COVER;
static float coverStrength = 0.5f, coverNoise = 0.0f, repaintStart = 0, repaintEnd = 10;
static int addTrack = 2;
static const char* TRACKS[] = {"vocals", "backing_vocals", "drums", "bass", "guitar", "keyboard", "percussion", "strings", "synth", "fx", "brass", "woodwinds"};
static const char* TRACK_NAMES[] = {"Vocals", "Backing vocals", "Drums", "Bass", "Guitar", "Piano / keys", "Percussion", "Strings", "Synth", "FX", "Brass", "Woodwinds"};
static const int TRACK_COUNT = 12;
static bool stemPick[TRACK_COUNT] = {true, false, true, true, true, true, false, false, false, false, false, false};

static int baseModelIndex() {
    for (size_t i = 0; i < engineModels.dit.size(); ++i) if (engineModels.dit[i].find("-base-") != std::string::npos) return (int)i;
    return -1;
}

static void remix() {
    if (sourcePath.empty()) { say("Drop a song in first"); return; }
    const Settings s = st;
    const int mode = remixMode;
    const float strength = coverStrength, noise = coverNoise, rs = repaintStart, re = repaintEnd;
    const int track = addTrack, base = baseModelIndex();
    const std::string title = fs::u8path(sourcePath).stem().u8string();
    queue([s, mode, strength, noise, rs, re, track, base, title] {
        std::string err, bytes, name;
        if (!ensureEngine()) return;
        if (!sourceBytes(bytes, name)) { say("Can't read the song"); return; }
        static const char* tasks[] = {"cover", "cover-nofsq", "repaint", "complete", "lego"};
        Json::Writer w = baseRequest(s);
        w.s("task_type", tasks[mode]);
        if (mode <= RM_CLOSE) w.f("audio_cover_strength", strength).f("cover_noise_strength", noise);
        if (mode == RM_REPAINT || mode == RM_LEGO) w.f("repainting_start", rs).f("repainting_end", re);
        if (mode >= RM_COMPLETE) {
            w.s("track", TRACKS[track]);
            if (base < 0) { say("This needs the Base song model: get it in the Models tab"); return; }
            w.s("synth_model", engineModels.dit[base]);
        }
        std::string id;
        if (!Engine::submitAudio("/synth", w.str(), bytes, name, id, err, mode == RM_CLOSE ? bytes : "")) { say(err); return; }
        static const char* verbs[] = {"Covering it", "Remixing it", "Repainting it", "Completing it", "Adding the track"};
        if (!waitJob(id, verbs[mode], err)) { say(err); return; }
        static const char* kinds[] = {"cover", "remix", "repaint", "complete", "lego"};
        Json::Value m; m.type = Json::Value::OBJ;
        m.o.emplace_back("caption", Json::Value{Json::Value::STR, false, 0, s.caption});
        collect(Engine::result(id), {m}, kinds[mode], title + " - " + kinds[mode]);
        say("Done");
    });
}

static void splitStems() {
    if (sourcePath.empty()) { say("Drop a song in first"); return; }
    const int base = baseModelIndex();
    if (base < 0) { say("Splitting needs the Base song model: get it in the Models tab"); return; }
    std::vector<int> picks;
    for (int i = 0; i < TRACK_COUNT; ++i) if (stemPick[i]) picks.push_back(i);
    if (picks.empty()) { say("Pick the stems to split out"); return; }
    const std::string title = fs::u8path(sourcePath).stem().u8string(), model = engineModels.dit[base];
    const Settings s = st;
    queue([picks, title, model, s] {
        std::string err, bytes, name;
        if (!ensureEngine()) return;
        if (!sourceBytes(bytes, name)) { say("Can't read the song"); return; }
        int done = 0;
        for (int t : picks) {
            if (stopJob) break;
            Json::Writer w;
            w.s("task_type", "extract").s("track", TRACKS[t]).s("caption", s.caption).s("synth_model", model).s("output_format", "wav16");
            w.i("seed", s.seed).i("inference_steps", s.steps).f("guidance_scale", s.guidance).f("shift", s.shift).s("solver", SOLVERS[s.solver]);
            std::string id;
            if (!Engine::submitAudio("/synth", w.str(), bytes, name, id, err)) { say(err); return; }
            progress = (float)done / picks.size();
            if (!waitJob(id, std::string("Splitting out the ") + TRACK_NAMES[t], err)) { say(err); return; }
            collect(Engine::result(id), {}, std::string("stem: ") + TRACK_NAMES[t], title + " - " + TRACK_NAMES[t]);
            ++done;
        }
        say("Done: " + std::to_string(done) + " stem" + (done == 1 ? "" : "s"));
    });
}

static void askEngine() {
    if (sourcePath.empty()) { say("Drop a song in first"); return; }
    queue([] {
        std::string err, bytes, name;
        if (!ensureEngine()) return;
        if (!sourceBytes(bytes, name)) { say("Can't read the song"); return; }
        std::string id;
        if (!Engine::submitAudio("/understand", "", bytes, name, id, err)) { say(err); return; }
        if (!waitJob(id, "Listening to it", err)) { say(err); return; }
        const auto parts = Http::splitMixed(Engine::result(id));
        for (const auto& p : parts) {
            if (p.type.find("json") == std::string::npos) continue;
            Json::Value v, m;
            if (!Json::parse(p.data, v)) continue;
            m = v.type == Json::Value::ARR && !v.a.empty() ? v.a[0] : v;
            std::string out;
            auto add = [&](const char* label, const std::string& val) { if (!val.empty() && val != "0") out += std::string(label) + ": " + val + "\n"; };
            add("BPM", m.str("bpm")); add("Key", m.str("keyscale")); add("Time signature", m.str("timesignature").empty() ? "" : m.str("timesignature") + "/4");
            add("Length", m.str("duration").empty() ? "" : m.str("duration") + " s"); add("Language", m.str("vocal_language"));
            add("Style", m.str("caption"));
            if (!m.str("lyrics").empty()) out += "\nLyrics:\n" + m.str("lyrics") + "\n";
            post([out] { understood = out; });
        }
        say("ACE-Step has listened to it");
    });
}

// ---------- models ----------
struct Fetch { std::atomic<float> done{-1.0f}; std::string err; };
static Fetch fetches[64];
static std::atomic<bool> stopFetch{false};

static bool haveModel(const char* file) { std::error_code ec; return fs::exists(fs::u8path(Engine::modelsDir()) / file, ec); }

static void refreshModels() {
    queue([] {
        std::string err;
        if (!ensureEngine()) return;
        Engine::Models m;
        if (Engine::models(m, err)) post([m] { engineModels = m; });
        say(m.lm.empty() ? "Ready (no songwriter yet: songs need your own lyrics, or get one in the Models tab)" : "Ready");
    });
}

static void fetchModels(const std::vector<int>& which) {
    stopFetch = false;
    std::thread([which] {
        for (int i : which) {
            const auto& m = Catalog::models()[i];
            if (haveModel(m.file)) continue;
            fetches[i].done = 0;
            std::string err;
            const std::string dest = (fs::u8path(Engine::modelsDir()) / m.file).u8string();
            const bool ok = Http::download(Catalog::url(m.file), dest, [i](int64_t d, int64_t t) { fetches[i].done = t > 0 ? (float)d / t : 0.0f; }, &stopFetch, err);
            fetches[i].done = -1;
            if (!ok) { say(std::string(m.file) + ": " + err); return; }
        }
        // the engine scans the folder when it starts: restart it to see the new ones
        Engine::stop();
        refreshModels();
    }).detach();
}

// ---------- the player ----------
static SDL_AudioDeviceID audioDev = 0;
static std::atomic<size_t> playPos{0};
static std::atomic<bool> playing{false};
static const Audio::Clip* playClip = nullptr;
static std::mutex playLock;
static float volume = 0.9f;

static void audioCallback(void*, Uint8* stream, int len) {
    float* out = (float*)stream;
    const int n = len / (int)sizeof(float);
    std::lock_guard<std::mutex> g(playLock);
    for (int i = 0; i < n; i += 2) {
        float l = 0, r = 0;
        if (playing && playClip && !playClip->empty()) {
            const size_t frames = playClip->samples.size() / playClip->channels;
            size_t p = playPos;
            // the clip's rate to 48 kHz: nearest (Hymnal's songs are 48 kHz already)
            const size_t f = (size_t)((double)p * playClip->rate / 48000.0);
            if (f >= frames) playing = false;
            else {
                l = playClip->samples[f * playClip->channels];
                r = playClip->channels > 1 ? playClip->samples[f * playClip->channels + 1] : l;
                playPos = p + 1;
            }
        }
        out[i] = l * volume; out[i + 1] = r * volume;
    }
}

static void play(const Audio::Clip* c, bool restart) {
    std::lock_guard<std::mutex> g(playLock);
    if (playClip != c || restart) playPos = 0;
    playClip = c;
    playing = c != nullptr;
}

// ---------- the frame ----------
static SDL_Window* window = nullptr;
static bool quit = false, showAbout = false;
static int tab = 0, forcedTab = -1;

static void heading(const char* t) { ImGui::PushStyleColor(ImGuiCol_Text, col4(C_PINK)); ImGui::TextUnformatted(t); ImGui::PopStyleColor(); }

static void waveform(const std::vector<float>& lo, const std::vector<float>& hi, ImVec2 size, double seconds, bool isPlaying, size_t pos48k, std::function<void(double)> seek) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 a = ImGui::GetCursorScreenPos(), b(a.x + size.x, a.y + size.y);
    dl->AddRectFilled(a, b, C_WELL);
    const float mid = (a.y + b.y) / 2, half = size.y / 2 - 2;
    const double frac = seconds > 0 ? std::min(1.0, pos48k / 48000.0 / seconds) : 0;
    for (size_t x = 0; x < lo.size(); ++x) {
        const float px = a.x + size.x * x / lo.size();
        const ImU32 col = (double)x / lo.size() < frac ? C_PINK : IM_COL32(0x6a, 0x6a, 0x6a, 255);
        dl->AddLine(ImVec2(px, mid - hi[x] * half), ImVec2(px, mid - lo[x] * half + 1), col, std::max(1.0f, size.x / lo.size()));
    }
    if (isPlaying || frac > 0) dl->AddLine(ImVec2(a.x + size.x * (float)frac, a.y), ImVec2(a.x + size.x * (float)frac, b.y), C_TEXT, 1.5f);
    ImGui::InvisibleButton("wave", size);
    if (ImGui::IsItemActive() && seek && seconds > 0) seek(std::clamp((ImGui::GetIO().MousePos.x - a.x) / size.x, 0.0f, 1.0f) * seconds);
    if (ImGui::IsItemHovered() && seek) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
}

static void dropZone(const char* line1, const char* line2, float h) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float w = ImGui::GetContentRegionAvail().x;
    const ImVec2 a = ImGui::GetCursorScreenPos(), b(a.x + w, a.y + h);
    for (float x = a.x; x < b.x; x += 12) { dl->AddLine(ImVec2(x, a.y), ImVec2(std::min(x + 6, b.x), a.y), C_LINE, 1.5f); dl->AddLine(ImVec2(x, b.y), ImVec2(std::min(x + 6, b.x), b.y), C_LINE, 1.5f); }
    for (float y = a.y; y < b.y; y += 12) { dl->AddLine(ImVec2(a.x, y), ImVec2(a.x, std::min(y + 6, b.y)), C_LINE, 1.5f); dl->AddLine(ImVec2(b.x, y), ImVec2(b.x, std::min(y + 6, b.y)), C_LINE, 1.5f); }
    dl->AddText(ImVec2(a.x + (w - ImGui::CalcTextSize(line1).x) / 2, a.y + h / 2 - 18), C_TEXT, line1);
    dl->AddText(ImVec2(a.x + (w - ImGui::CalcTextSize(line2).x) / 2, a.y + h / 2 + 4), C_DIM, line2);
    ImGui::InvisibleButton("drop", ImVec2(w, h));
    if (ImGui::IsItemClicked()) { const std::string p = fileDialog(false, AUDIO, nullptr, ""); if (!p.empty()) loadSource(p); }
}

static void sourceBlock() {
    if (sourcePath.empty()) { dropZone("Drop a song here (WAV, MP3, FLAC...)", "or click to open one", 120); return; }
    ImGui::Text("%s", fs::u8path(sourcePath).filename().u8string().c_str());
    ImGui::SameLine(); if (ImGui::SmallButton("Change...")) { const std::string p = fileDialog(false, AUDIO, nullptr, sourcePath); if (!p.empty()) loadSource(p); }
    ImGui::SameLine(); if (ImGui::SmallButton(playing && playClip == &sourceClip ? "Stop" : "Play")) play(playing && playClip == &sourceClip ? nullptr : &sourceClip, true);
    if (!sourceLo.empty()) waveform(sourceLo, sourceHi, ImVec2(ImGui::GetContentRegionAvail().x, 70), sourceClip.seconds(), playing && playClip == &sourceClip, playClip == &sourceClip ? playPos.load() : 0,
                                    [](double t) { play(&sourceClip, false); playPos = (size_t)(t * 48000); });
    if (sourceMeasuredOk) ImGui::TextDisabled("%.0f s   about %.0f BPM   %s (or %s)", sourceClip.seconds(), sourceMeasured.bpm, sourceMeasured.key.c_str(), sourceMeasured.keyAlt.c_str());
}

static void createTab() {
    const float w = ImGui::GetContentRegionAvail().x;
    heading("STYLE");
    ImGui::TextDisabled("Genre, mood, instruments, voice: \"dark synthwave, female vocals, driving bass\"");
    ImGui::InputTextMultiline("##caption", st.caption, sizeof st.caption, ImVec2(w, 64));
    ImGui::Spacing();
    heading("LYRICS");
    ImGui::RadioButton("My lyrics", &st.lyricsMode, LY_MINE); ImGui::SameLine();
    ImGui::RadioButton("Write them for me", &st.lyricsMode, LY_WRITE); ImGui::SameLine();
    ImGui::RadioButton("Instrumental", &st.lyricsMode, LY_INSTRUMENTAL);
    if (st.lyricsMode == LY_MINE) {
        for (const char* tag : {"[Intro]", "[Verse]", "[Pre-Chorus]", "[Chorus]", "[Bridge]", "[Breakdown]", "[Outro]"}) {
            if (ImGui::SmallButton(tag)) { size_t n = strlen(st.lyrics); const std::string add = std::string(n && st.lyrics[n - 1] != '\n' ? "\n\n" : "\n") + tag + "\n"; if (n + add.size() < sizeof st.lyrics) strcat(st.lyrics, add.c_str()); }
            ImGui::SameLine();
        }
        ImGui::NewLine();
        ImGui::InputTextMultiline("##lyrics", st.lyrics, sizeof st.lyrics, ImVec2(w, std::max(160.0f, ImGui::GetContentRegionAvail().y - 70)));
    } else ImGui::TextDisabled(st.lyricsMode == LY_WRITE ? "The songwriter writes the lyrics from your style." : "No vocals.");
    ImGui::Spacing();
    ImGui::BeginDisabled(busy);
    if (ImGui::Button(st.variations > 1 ? "Make the songs" : "Make the song", ImVec2(w, 36))) createSong();
    ImGui::EndDisabled();
}

static void remixTab() {
    const float w = ImGui::GetContentRegionAvail().x;
    heading("THE SONG");
    sourceBlock();
    ImGui::Spacing();
    heading("WHAT TO DO");
    static const char* modes[] = {"Cover: a new take in your style (loose)", "Remix: stays close to the original", "Repaint: redo a section",
                                  "Complete: build a full song round one stem (needs Base)", "Add a track (needs Base)"};
    ImGui::PushItemWidth(w);
    ImGui::Combo("##mode", &remixMode, modes, RM_COUNT);
    if (remixMode <= RM_CLOSE) {
        ImGui::SliderFloat("##str", &coverStrength, 0.0f, 1.0f, "how much of the original: %.2f");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("How many steps follow the original. Lower: freer. Remix works well at 0.2-0.5.");
        ImGui::SliderFloat("##noise", &coverNoise, 0.0f, 1.0f, "start from the original: %.2f");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("0 starts from noise; nearer 1 starts nearly as the original.");
    }
    if (remixMode == RM_REPAINT || remixMode == RM_LEGO) {
        const float len = (float)std::max(1.0, sourceClip.seconds());
        ImGui::SliderFloat("##rs", &repaintStart, -30.0f, len, "from %.1f s");
        ImGui::SliderFloat("##re", &repaintEnd, 0.0f, len + 30, "to %.1f s");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Before 0 or past the end: it writes new music there (out-painting).");
    }
    if (remixMode >= RM_COMPLETE) ImGui::Combo("##track", &addTrack, TRACK_NAMES, TRACK_COUNT);
    ImGui::PopItemWidth();
    ImGui::TextDisabled("The style box on the Create tab steers it.");
    ImGui::Spacing();
    ImGui::BeginDisabled(busy || sourcePath.empty());
    if (ImGui::Button("Go", ImVec2(w, 36))) remix();
    ImGui::EndDisabled();
}

static void stemsTab() {
    const float w = ImGui::GetContentRegionAvail().x;
    heading("THE SONG");
    sourceBlock();
    ImGui::Spacing();
    heading("SPLIT OUT");
    for (int i = 0; i < TRACK_COUNT; ++i) { if (i % 3) ImGui::SameLine(w / 3 * (i % 3)); ImGui::Checkbox(TRACK_NAMES[i], &stemPick[i]); }
    ImGui::Spacing();
    if (baseModelIndex() < 0) ImGui::TextColored(col4(C_PINK), "Splitting needs the Base song model (1.4-1.9 GB): get it in the Models tab.");
    ImGui::TextDisabled("Each stem is drawn out by ACE-Step's Base model, one at a time, and lands in your songs.");
    ImGui::BeginDisabled(busy || sourcePath.empty() || baseModelIndex() < 0);
    if (ImGui::Button("Split it", ImVec2(w, 36))) splitStems();
    ImGui::EndDisabled();
}

static void analyseTab() {
    const float w = ImGui::GetContentRegionAvail().x;
    heading("THE SONG");
    sourceBlock();
    ImGui::Spacing();
    if (sourceMeasuredOk) {
        heading("MEASURED");
        ImGui::Text("Tempo: %.1f BPM", sourceMeasured.bpm);
        ImGui::SameLine(w / 2); ImGui::Text("Key: %s", sourceMeasured.key.c_str());
        ImGui::TextDisabled("(half or double: %.1f / %.1f)", sourceMeasured.bpm / 2, sourceMeasured.bpm * 2);
        ImGui::SameLine(w / 2); ImGui::TextDisabled("(or %s)", sourceMeasured.keyAlt.c_str());
        ImGui::Text("Loudness: %.1f dB RMS, peak %.1f dB", sourceMeasured.loudnessDb, sourceMeasured.peakDb);
        ImGui::Spacing();
    }
    heading("ASK ACE-STEP");
    ImGui::TextDisabled("It listens and writes down the BPM, key, time signature, style and lyrics it hears.");
    ImGui::BeginDisabled(busy || sourcePath.empty());
    if (ImGui::Button("Listen to it", ImVec2(w, 32))) askEngine();
    ImGui::EndDisabled();
    if (!understood.empty()) {
        ImGui::BeginChild("##understood", ImVec2(w, 0), true);
        ImGui::TextWrapped("%s", understood.c_str());
        ImGui::EndChild();
    }
}

static void modelsTab() {
    const float w = ImGui::GetContentRegionAvail().x;
    heading("MODELS");
    ImGui::TextDisabled("Kept in: %s", Engine::modelsDir().c_str());
    ImGui::SameLine();
    if (ImGui::SmallButton("Open")) {
#ifdef _WIN32
        std::error_code ec; fs::create_directories(fs::u8path(Engine::modelsDir()), ec);
        ShellExecuteW(nullptr, L"open", fs::u8path(Engine::modelsDir()).wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#endif
    }
    std::vector<int> starter;
    for (size_t i = 0; i < Catalog::models().size(); ++i) if (Catalog::models()[i].starter && !haveModel(Catalog::models()[i].file)) starter.push_back((int)i);
    if (!starter.empty()) {
        int mb = 0; for (int i : starter) mb += Catalog::models()[i].mb;
        char l[96]; snprintf(l, sizeof l, "Get the starter set (%.1f GB)", mb / 1024.0);
        if (ImGui::Button(l, ImVec2(w, 32))) fetchModels(starter);
    }
    ImGui::BeginChild("##models", ImVec2(w, ImGui::GetContentRegionAvail().y - 44), false);
    int lastRole = -1;
    for (size_t i = 0; i < Catalog::models().size(); ++i) {
        const auto& m = Catalog::models()[i];
        if (m.role != lastRole) { ImGui::Spacing(); heading(Catalog::roleName(m.role)); lastRole = m.role; }
        ImGui::PushID((int)i);
        const bool have = haveModel(m.file);
        ImGui::Text("%s", m.file);
        ImGui::SameLine(w - 150);
        const float d = fetches[i].done;
        if (d >= 0) ImGui::ProgressBar(d, ImVec2(140, 0));
        else if (have) ImGui::TextColored(col4(C_PINK), "on this PC");
        else { char l[48]; snprintf(l, sizeof l, "Get (%.1f GB)", m.mb / 1024.0); if (ImGui::Button(l, ImVec2(140, 0))) fetchModels({(int)i}); }
        ImGui::TextDisabled("%s%s", m.about, m.starter ? "  (starter)" : "");
        ImGui::PopID();
    }
    ImGui::Spacing();
    ImGui::TextDisabled("From huggingface.co/Serveurperso/ACE-Step-1.5-GGUF (ACE-Step 1.5, Apache-2.0).");
    ImGui::EndChild();
}

static void settingsPanel(ImVec2 pos, ImVec2 size) {
    ImGui::SetNextWindowPos(pos); ImGui::SetNextWindowSize(size);
    ImGui::Begin("##settings", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
    ImGui::BeginChild("##s", ImVec2(0, 0), false);
    const float w = ImGui::GetContentRegionAvail().x;
    ImGui::PushItemWidth(w);
    auto tip = [](const char* t) { if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", t); };
    heading("THE SONG");
    ImGui::SliderInt("##dur", &st.duration, 0, 600, st.duration == 0 ? "length: let it choose" : "length: %d s"); tip("0: the songwriter picks. Up to 10 minutes.");
    ImGui::SliderInt("##bpm", &st.bpm, 0, 220, st.bpm == 0 ? "tempo: let it choose" : "tempo: %d BPM"); tip("0: the songwriter picks.");
    ImGui::Combo("##key", &st.key, KEYS, 25);
    static const char* sigs[] = {"time: let it choose", "time: 2/4", "time: 3/4", "time: 4/4", "time: 6/8"};
    int sig = st.timeSig == 0 ? 0 : st.timeSig == 2 ? 1 : st.timeSig == 3 ? 2 : st.timeSig == 4 ? 3 : 4;
    if (ImGui::Combo("##sig", &sig, sigs, 5)) { static const int v[] = {0, 2, 3, 4, 6}; st.timeSig = v[sig]; }
    if (ImGui::BeginCombo("##lang", LANGS[st.language][0])) {
        for (int i = 0; i < (int)(sizeof LANGS / sizeof LANGS[0]); ++i) if (ImGui::Selectable(LANGS[i][0], st.language == i)) st.language = i;
        ImGui::EndCombo();
    }
    tip("The language it sings in");
    ImGui::SliderInt("##var", &st.variations, 1, 4, "variations: %d"); tip("How many different songs from the one prompt");
    int seed = (int)st.seed;
    ImGui::SetNextItemWidth(w - 70);
    if (ImGui::DragInt("##seed", &seed, 1.0f, -1, 2147483647, seed < 0 ? "seed: random" : "seed: %d")) st.seed = seed;
    tip("-1: a new song every time. A number: the same again.");
    ImGui::SameLine(); if (ImGui::Button("Dice", ImVec2(-1, 0))) st.seed = std::uniform_int_distribution<int>(0, 2147483646)(*new std::mt19937(std::random_device{}()));

    ImGui::Spacing(); heading("THE SONGWRITER");
    ImGui::SliderFloat("##temp", &st.temperature, 0.1f, 1.5f, "creativity: %.2f"); tip("Lower: safer, more predictable. Higher: wilder.");
    ImGui::SliderFloat("##cfg", &st.lmCfg, 1.0f, 5.0f, "follow the style: %.2f"); tip("How strictly it follows your style (1: off).");
    ImGui::SliderFloat("##topp", &st.topP, 0.1f, 1.0f, "top-p: %.2f");
    ImGui::SliderInt("##topk", &st.topK, 0, 200, st.topK == 0 ? "top-k: off" : "top-k: %d");
    ImGui::Checkbox("Enrich my style text", &st.enrichCaption); tip("Let the songwriter expand your style into a fuller description.");
    ImGui::InputTextWithHint("##neg", "avoid: e.g. autotune, screaming", st.negative, sizeof st.negative);
    if (!engineModels.lm.empty()) {
        if (ImGui::BeginCombo("##lmm", st.lmModel < 0 ? "songwriter: the engine's choice" : engineModels.lm[st.lmModel].c_str())) {
            if (ImGui::Selectable("the engine's choice", st.lmModel < 0)) st.lmModel = -1;
            for (int i = 0; i < (int)engineModels.lm.size(); ++i) if (ImGui::Selectable(engineModels.lm[i].c_str(), st.lmModel == i)) st.lmModel = i;
            ImGui::EndCombo();
        }
    }

    ImGui::Spacing(); heading("THE SINGER AND BAND");
    ImGui::SliderInt("##steps", &st.steps, 0, 100, st.steps == 0 ? "steps: the model's own" : "steps: %d"); tip("0: turbo 8, SFT and Base 50. More: finer, slower.");
    ImGui::SliderFloat("##guid", &st.guidance, 0.0f, 10.0f, st.guidance == 0 ? "guidance: auto" : "guidance: %.1f"); tip("Turbo models always use 1.");
    ImGui::SliderFloat("##shift", &st.shift, 0.0f, 6.0f, st.shift == 0 ? "shift: auto" : "shift: %.2f"); tip("The noise schedule: turbo 3, SFT/Base 1.");
    ImGui::Combo("##solver", &st.solver, SOLVER_NAMES, 4); tip("euler: steady. sde: livelier. dpm3m / stork4: higher order.");
    ImGui::SliderFloat("##peak", &st.peakClip, 1.0f, 20.0f, "peak clip: %.1f");
    if (!engineModels.dit.empty()) {
        if (ImGui::BeginCombo("##ditm", st.songModel < 0 ? "song model: the engine's choice" : engineModels.dit[st.songModel].c_str())) {
            if (ImGui::Selectable("the engine's choice", st.songModel < 0)) st.songModel = -1;
            for (int i = 0; i < (int)engineModels.dit.size(); ++i) if (ImGui::Selectable(engineModels.dit[i].c_str(), st.songModel == i)) st.songModel = i;
            ImGui::EndCombo();
        }
    }
    ImGui::PopItemWidth();
    ImGui::EndChild();
    ImGui::End();
}

static void songsPanel(ImVec2 pos, ImVec2 size) {
    ImGui::SetNextWindowPos(pos); ImGui::SetNextWindowSize(size);
    ImGui::Begin("##songs", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
    heading("YOUR SONGS");
    if (songs.empty()) { ImGui::TextWrapped("Songs you make land here, and in %s.", songsDir().c_str()); }
    for (size_t i = 0; i < songs.size(); ++i) {
        ImGui::PushID((int)i);
        const Song& s = songs[i];
        char label[256];
        snprintf(label, sizeof label, "%s\n%s%.0f s%s%s", s.title.c_str(), s.kind == "song" ? "" : (s.kind + "  ").c_str(), s.seconds,
                 s.bpm > 0 ? ("  " + std::to_string((int)s.bpm) + " BPM").c_str() : "", s.key.empty() ? "" : ("  " + s.key).c_str());
        if (ImGui::Selectable(label, current == (int)i, 0, ImVec2(0, 40))) { current = (int)i; play(&songs[i].clip, true); }
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Use for remix / stems / analysis")) loadSource(s.path);
            if (ImGui::MenuItem("Show in folder")) {
#ifdef _WIN32
                const std::wstring arg = L"/select,\"" + fs::u8path(s.path).wstring() + L"\"";
                ShellExecuteW(nullptr, L"open", L"explorer.exe", arg.c_str(), nullptr, SW_SHOWNORMAL);
#endif
            }
            if (ImGui::MenuItem("Use its lyrics and style")) {
                snprintf(st.caption, sizeof st.caption, "%s", s.caption.c_str());
                snprintf(st.lyrics, sizeof st.lyrics, "%s", s.lyrics.c_str());
                st.lyricsMode = LY_MINE;
            }
            if (ImGui::MenuItem("Remove from the list")) { if (playClip == &songs[i].clip) play(nullptr, true); songs.erase(songs.begin() + i); if (current >= (int)songs.size()) current = (int)songs.size() - 1; ImGui::EndPopup(); ImGui::PopID(); break; }
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }
    ImGui::End();
}

static void playerBar(ImVec2 pos, ImVec2 size) {
    ImGui::SetNextWindowPos(pos); ImGui::SetNextWindowSize(size);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, col4(C_BG));
    ImGui::Begin("##player", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
    if (current >= 0 && current < (int)songs.size()) {
        Song& s = songs[current];
        const bool mine = playClip == &s.clip;
        if (ImGui::Button(mine && playing ? "Pause" : "Play", ImVec2(70, 0))) { if (mine && playing) playing = false; else { if (!mine) play(&s.clip, true); else playing = true; } }
        ImGui::SameLine();
        if (ImGui::Button("Save as...", ImVec2(90, 0))) {
            const std::string p = fileDialog(true, "WAV (16-bit)|*.wav|WAV (24-bit)|*.wav|MP3 (needs ffmpeg)|*.mp3|", "wav", s.path);
            if (!p.empty()) {
                std::string err, ext = fs::u8path(p).extension().u8string();
                for (auto& c : ext) c = (char)std::tolower((unsigned char)c);
                if (ext == ".mp3") {
                    const std::string ff = Audio::ffmpeg();
                    if (ff.empty()) say("MP3 needs ffmpeg on the PATH");
                    else {
#ifdef _WIN32
                        const std::wstring cmd = L"\"\"" + fs::u8path(ff).wstring() + L"\" -v error -y -i \"" + fs::u8path(s.path).wstring() + L"\" -b:a 320k \"" + fs::u8path(p).wstring() + L"\"\"";
                        say(_wsystem(cmd.c_str()) == 0 ? "Saved " + p : "ffmpeg couldn't write " + p);
#endif
                    }
                } else say(Audio::saveWav(p, s.clip, 16, err) ? "Saved " + p : err);
            }
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110);
        ImGui::SliderFloat("##vol", &volume, 0.0f, 1.0f, "volume %.2f");
        ImGui::SameLine();
        const double t = mine ? playPos / 48000.0 : 0;
        ImGui::Text("%d:%02d / %d:%02d   %s", (int)t / 60, (int)t % 60, (int)s.seconds / 60, (int)s.seconds % 60, s.title.c_str());
        waveform(s.lo, s.hi, ImVec2(ImGui::GetContentRegionAvail().x, size.y - 54), s.seconds, mine && playing, mine ? playPos.load() : 0,
                 [&s](double t2) { play(&s.clip, false); playPos = (size_t)(t2 * 48000); });
    } else ImGui::TextDisabled("Nothing playing yet.");
    ImGui::End();
    ImGui::PopStyleColor();
}

static void menuBar() {
    if (!ImGui::BeginMainMenuBar()) return;
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Open a song...")) { const std::string p = fileDialog(false, AUDIO, nullptr, ""); if (!p.empty()) loadSource(p); }
        if (ImGui::MenuItem("Show your songs")) {
#ifdef _WIN32
            std::error_code ec; fs::create_directories(fs::u8path(songsDir()), ec);
            ShellExecuteW(nullptr, L"open", fs::u8path(songsDir()).wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#endif
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Quit", "Ctrl+Q")) quit = true;
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Engine")) {
        if (ImGui::MenuItem("Restart it")) { Engine::stop(); refreshModels(); }
        if (ImGui::MenuItem("Show its log")) {
#ifdef _WIN32
            ShellExecuteW(nullptr, L"open", fs::u8path(Engine::logPath()).wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#endif
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Help")) { if (ImGui::MenuItem("About Hymnal")) showAbout = true; ImGui::EndMenu(); }
    ImGui::EndMainMenuBar();
}

static void about() {
    if (showAbout) { ImGui::OpenPopup("About Hymnal"); showAbout = false; }
    ImGui::SetNextWindowSize(ImVec2(Splash::W + 16, 0));
    if (ImGui::BeginPopupModal("About Hymnal", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar)) {
        const ImVec2 at = ImGui::GetCursorScreenPos();
        Splash::draw(ImGui::GetWindowDrawList(), at.x, at.y, nullptr, -1);
        ImGui::Dummy(ImVec2(Splash::W, Splash::H));
        ImGui::TextWrapped("Whole songs, with vocals, from a style and your lyrics, on your own PC. Part of Atelier, Cursed Entertainment's creative suite.");
        ImGui::TextDisabled("ACE-Step 1.5 (ACE Studio and StepFun, Apache-2.0), run by acestep.cpp (MIT, on ggml).");
        if (ImGui::Button("Close", ImVec2(-1, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

static void frame() {
    std::vector<std::function<void()>> todo;
    { std::lock_guard<std::mutex> g(mtx); todo.swap(onMain); }
    for (auto& f : todo) f();
    ImGuiIO& io = ImGui::GetIO();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Q)) quit = true;
    if (!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Space) && current >= 0) { Song& s = songs[current]; if (playClip == &s.clip) playing = !playing; else play(&s.clip, true); }
    menuBar();
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float top = ImGui::GetFrameHeight(), side = 280, right = 300, player = 120;
    songsPanel(ImVec2(vp->Pos.x, vp->Pos.y + top), ImVec2(side, vp->Size.y - top - player));
    settingsPanel(ImVec2(vp->Pos.x + vp->Size.x - right, vp->Pos.y + top), ImVec2(right, vp->Size.y - top - player));
    playerBar(ImVec2(vp->Pos.x, vp->Pos.y + vp->Size.y - player), ImVec2(vp->Size.x, player));
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + side, vp->Pos.y + top));
    ImGui::SetNextWindowSize(ImVec2(vp->Size.x - side - right, vp->Size.y - top - player));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, col4(C_BG));
    ImGui::Begin("##main", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);
    if (ImGui::BeginTabBar("##tabs")) {
        const char* names[] = {"Create", "Remix", "Stems", "Analyse", "Models"};
        for (int i = 0; i < 5; ++i) {
            ImGuiTabItemFlags fl = forcedTab == i ? ImGuiTabItemFlags_SetSelected : 0;
            if (ImGui::BeginTabItem(names[i], nullptr, fl)) {
                tab = i;
                ImGui::Spacing();
                if (i == 0) createTab(); else if (i == 1) remixTab(); else if (i == 2) stemsTab(); else if (i == 3) analyseTab(); else modelsTab();
                ImGui::EndTabItem();
            }
        }
        forcedTab = -1;
        ImGui::EndTabBar();
    }
    // the status line, and Stop while something runs
    ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 34);
    ImGui::Separator();
    if (busy) { if (ImGui::SmallButton("Stop")) stopJob = true; ImGui::SameLine(); }
    { std::lock_guard<std::mutex> g(mtx); ImGui::TextDisabled("%s", status.c_str()); }
    ImGui::End();
    ImGui::PopStyleColor();
    about();
}

static std::string songPrompt; static int songSeconds = 0;

int main(int argc, char** argv) {
    std::string capture, file;
    std::vector<std::string> args;
#ifdef _WIN32
    { int n; LPWSTR* wa = CommandLineToArgvW(GetCommandLineW(), &n); if (wa) { for (int i = 1; i < n; ++i) args.push_back(fs::path(wa[i]).u8string()); LocalFree(wa); } }
#else
    for (int i = 1; i < argc; ++i) args.push_back(argv[i]);
#endif
    (void)argc; (void)argv;
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--capture" && i + 1 < args.size()) capture = args[++i];
        else if (args[i] == "--tab" && i + 1 < args.size()) forcedTab = atoi(args[++i].c_str());
        else if (args[i] == "--song" && i + 1 < args.size()) songPrompt = args[++i];
        else if (args[i] == "--seconds" && i + 1 < args.size()) songSeconds = atoi(args[++i].c_str());
        else file = args[i];
    }
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) return 1;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    window = SDL_CreateWindow("Hymnal", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1500, 900, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_HIDDEN);
    if (!window) return 1;
    SDL_SetWindowMinimumSize(window, 1100, 650);
    SDL_GLContext gl = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl);
    SDL_GL_SetSwapInterval(1);
    char* bp = SDL_GetBasePath();
    SDL_Surface* icon = bp ? SDL_LoadBMP((std::string(bp) + "assets/icon.bmp").c_str()) : nullptr;
    if (bp) SDL_free(bp);
    if (icon) SDL_SetWindowIcon(window, icon);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    loadFont();
    applyStyle();

    // the player: 48 kHz stereo float
    SDL_AudioSpec want{}, have{};
    want.freq = 48000; want.format = AUDIO_F32SYS; want.channels = 2; want.samples = 1024; want.callback = audioCallback;
    audioDev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (audioDev) SDL_PauseAudioDevice(audioDev, 0);

    // the splash while the engine starts
    SDL_Window* splashWin = nullptr;
    if (capture.empty()) {
        splashWin = SDL_CreateWindow("Hymnal", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, (int)Splash::W, (int)Splash::H, SDL_WINDOW_OPENGL | SDL_WINDOW_BORDERLESS);
        if (splashWin && SDL_GL_MakeCurrent(splashWin, gl) != 0) { SDL_DestroyWindow(splashWin); splashWin = nullptr; SDL_GL_MakeCurrent(window, gl); }
        if (splashWin && icon) SDL_SetWindowIcon(splashWin, icon);
    }
    if (icon) SDL_FreeSurface(icon);
    ImGui_ImplSDL2_InitForOpenGL(splashWin ? splashWin : window, gl);
    ImGui_ImplOpenGL3_Init("#version 330");
    SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

    worker = std::thread(workerLoop);
    static std::atomic<bool> ready{false};
    queue([] {
        std::string err;
        say("Starting the engine...");
        if (!ensureEngine()) { ready = true; post([] { forcedTab = 4; }); return; }
        Engine::Models m;
        if (Engine::models(m, err)) post([m] { engineModels = m; });
        say(m.lm.empty() ? "Ready (no songwriter yet: get one in the Models tab)" : "Ready");
        ready = true;
    });
    if (!file.empty()) loadSource(file);
    if (!songPrompt.empty()) {   // after the engine's start job (the queue runs in order)
        snprintf(st.caption, sizeof st.caption, "%s", songPrompt.c_str());
        st.lyricsMode = LY_WRITE;
        if (songSeconds > 0) st.duration = songSeconds;
        createSong();
        forcedTab = 0;
    }
    if (splashWin) {
        const Uint32 start = SDL_GetTicks();
        bool skip = false;
        for (;;) {
            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                ImGui_ImplSDL2_ProcessEvent(&e);
                if (e.type == SDL_QUIT) quit = true;
                if (e.type == SDL_MOUSEBUTTONDOWN || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)) skip = true;
            }
            const Uint32 t = SDL_GetTicks() - start;
            if (skip || quit || (ready && t >= 1500) || t >= 20000) break;
            ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplSDL2_NewFrame(); ImGui::NewFrame();
            std::string line; { std::lock_guard<std::mutex> g(mtx); line = status; }
            Splash::draw(ImGui::GetBackgroundDrawList(), 0, 0, ready ? "Ready" : line.c_str(), ready ? 1.0f : std::min(0.9f, t / 8000.0f));
            ImGui::Render();
            int sw, sh; SDL_GL_GetDrawableSize(splashWin, &sw, &sh);
            glViewport(0, 0, sw, sh); glClearColor(0.1f, 0.1f, 0.1f, 1); glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            SDL_GL_SwapWindow(splashWin);
        }
        ImGui_ImplSDL2_Shutdown();
        SDL_DestroyWindow(splashWin);
        SDL_GL_MakeCurrent(window, gl);
        ImGui_ImplSDL2_InitForOpenGL(window, gl);
    }
    if (capture.empty()) { SDL_ShowWindow(window); SDL_MaximizeWindow(window); SDL_RaiseWindow(window); }

    int frames = 0, idle = 0;
    while (!quit) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            ImGui_ImplSDL2_ProcessEvent(&e);
            if (e.type == SDL_QUIT) quit = true;
            if (e.type == SDL_DROPFILE) { loadSource(e.drop.file); if (tab == 0 || tab == 4) forcedTab = 1; SDL_free(e.drop.file); }
        }
        ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplSDL2_NewFrame(); ImGui::NewFrame();
        frame();
        ImGui::Render();
        int w, h; SDL_GL_GetDrawableSize(window, &w, &h);
        glViewport(0, 0, w, h); glClearColor(0.1f, 0.1f, 0.1f, 1); glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        ++frames;
        if (!capture.empty()) {
            bool quiet; { std::lock_guard<std::mutex> g(mtx); quiet = jobs.empty() && onMain.empty(); }
            idle = (quiet && !busy) ? idle + 1 : 0;
            if (frames >= 10 && idle >= 6) {
                std::vector<unsigned char> px((size_t)w * h * 4);
                glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
                std::vector<unsigned char> flip(px.size());
                for (int y = 0; y < h; ++y) memcpy(&flip[(size_t)y * w * 4], &px[(size_t)(h - 1 - y) * w * 4], (size_t)w * 4);
                SDL_Surface* s = SDL_CreateRGBSurfaceWithFormatFrom(flip.data(), w, h, 32, w * 4, SDL_PIXELFORMAT_ABGR8888);
                SDL_SaveBMP(s, capture.c_str());
                SDL_FreeSurface(s);
                break;
            }
        }
        SDL_GL_SwapWindow(window);
    }
    stopJob = true; workerQuit = true; stopFetch = true;
    if (worker.joinable()) worker.join();
    if (audioDev) SDL_CloseAudioDevice(audioDev);
    Engine::stop();
    ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplSDL2_Shutdown(); ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl); SDL_DestroyWindow(window); SDL_Quit();
    return 0;
}
