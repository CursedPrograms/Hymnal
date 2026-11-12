// Hymnal - audio (see audio.h)
#include "audio.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace Audio {

// ---------- WAV ----------
static uint32_t u32(const std::string& d, size_t at) { return (uint8_t)d[at] | ((uint8_t)d[at + 1] << 8) | ((uint8_t)d[at + 2] << 16) | ((uint32_t)(uint8_t)d[at + 3] << 24); }
static uint16_t u16(const std::string& d, size_t at) { return (uint16_t)((uint8_t)d[at] | ((uint8_t)d[at + 1] << 8)); }

bool readWav(const std::string& d, Clip& out, std::string& err) {
    if (d.size() < 44 || d.compare(0, 4, "RIFF") || d.compare(8, 4, "WAVE")) { err = "Not a WAV file"; return false; }
    int fmt = 0, ch = 0, rate = 0, bits = 0;
    size_t pos = 12;
    while (pos + 8 <= d.size()) {
        const std::string id = d.substr(pos, 4);
        const uint32_t len = u32(d, pos + 4);
        const size_t body = pos + 8;
        if (id == "fmt ") {
            fmt = u16(d, body); ch = u16(d, body + 2); rate = (int)u32(d, body + 4); bits = u16(d, body + 14);
            if (fmt == 0xFFFE && len >= 26) fmt = u16(d, body + 24);   // WAVE_FORMAT_EXTENSIBLE: the sub-format's first word
        } else if (id == "data") {
            if (ch <= 0 || bits <= 0) { err = "WAV: no format before the data"; return false; }
            const size_t n = std::min<size_t>(len, d.size() - body) / (bits / 8);
            out.rate = rate; out.channels = ch; out.samples.resize(n);
            const char* p = d.data() + body;
            for (size_t i = 0; i < n; ++i) {
                float v = 0;
                if (fmt == 3 && bits == 32) memcpy(&v, p + i * 4, 4);
                else if (bits == 16) v = (int16_t)u16(d, body + i * 2) / 32768.0f;
                else if (bits == 24) { int32_t s = (uint8_t)p[i * 3] | ((uint8_t)p[i * 3 + 1] << 8) | ((int8_t)p[i * 3 + 2] << 16); v = s / 8388608.0f; }
                else if (bits == 32) v = (int32_t)u32(d, body + i * 4) / 2147483648.0f;
                else if (bits == 8) v = ((uint8_t)p[i] - 128) / 128.0f;
                out.samples[i] = v;
            }
            return true;
        }
        pos = body + len + (len & 1);
    }
    err = "WAV: no data";
    return false;
}

std::string wavBytes(const Clip& c, int bits) {
    const bool flt = bits == 32;
    const int bps = bits / 8;
    const uint32_t dataLen = (uint32_t)(c.samples.size() * bps);
    std::string o;
    auto w32 = [&](uint32_t v) { for (int i = 0; i < 4; ++i) o.push_back((char)((v >> (8 * i)) & 0xff)); };
    auto w16 = [&](uint16_t v) { o.push_back((char)(v & 0xff)); o.push_back((char)(v >> 8)); };
    o += "RIFF"; w32(36 + dataLen); o += "WAVEfmt "; w32(16); w16(flt ? 3 : 1); w16((uint16_t)c.channels);
    w32((uint32_t)c.rate); w32((uint32_t)(c.rate * c.channels * bps)); w16((uint16_t)(c.channels * bps)); w16((uint16_t)bits);
    o += "data"; w32(dataLen);
    o.reserve(o.size() + dataLen);
    for (float v : c.samples) {
        v = std::clamp(v, -1.0f, 1.0f);
        if (flt) { char b[4]; memcpy(b, &v, 4); o.append(b, 4); }
        else if (bits == 24) { const int32_t s = (int32_t)std::lround(v * 8388607.0f); o.push_back((char)(s & 0xff)); o.push_back((char)((s >> 8) & 0xff)); o.push_back((char)((s >> 16) & 0xff)); }
        else w16((uint16_t)(int16_t)std::lround(v * 32767.0f));
    }
    return o;
}

static bool readFile(const std::string& path, std::string& out) {
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

bool saveWav(const std::string& path, const Clip& c, int bits, std::string& err) {
    const std::string b = wavBytes(c, bits);
#ifdef _WIN32
    FILE* f = _wfopen(fs::u8path(path).wstring().c_str(), L"wb");
#else
    FILE* f = fopen(path.c_str(), "wb");
#endif
    if (!f) { err = "Can't write " + path; return false; }
    const bool ok = fwrite(b.data(), 1, b.size(), f) == b.size();
    fclose(f);
    if (!ok) err = "Couldn't write all of " + path;
    return ok;
}

std::string ffmpeg() {
    static std::string found;
    static bool looked = false;
    if (looked) return found;
    looked = true;
    std::error_code ec;
#ifdef _WIN32
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    std::vector<fs::path> spots = {fs::path(exe).parent_path() / "ffmpeg.exe", "C:/ffmpeg/bin/ffmpeg.exe"};
    wchar_t buf[MAX_PATH];
    if (SearchPathW(nullptr, L"ffmpeg.exe", nullptr, MAX_PATH, buf, nullptr)) spots.insert(spots.begin() + 1, fs::path(buf));
    if (const char* la = std::getenv("LOCALAPPDATA")) {
        const fs::path pk = fs::u8path(la) / "Microsoft" / "WinGet" / "Packages";
        if (fs::exists(pk, ec))
            for (const auto& d : fs::directory_iterator(pk, ec))
                if (d.path().filename().u8string().rfind("Gyan.FFmpeg", 0) == 0)
                    for (const auto& b : fs::directory_iterator(d.path(), ec)) spots.push_back(b.path() / "bin" / "ffmpeg.exe");
    }
    for (const auto& p : spots) if (fs::exists(p, ec)) { found = p.u8string(); break; }
#endif
    return found;
}

bool load(const std::string& path, Clip& out, std::string& err) {
    std::string ext = fs::u8path(path).extension().u8string();
    for (auto& ch : ext) ch = (char)std::tolower((unsigned char)ch);
    if (ext == ".wav") {
        std::string d;
        if (!readFile(path, d)) { err = "Can't open " + path; return false; }
        if (readWav(d, out, err)) return true;
    }
    // everything else (MP3, FLAC, OGG, M4A...) through ffmpeg: 48 kHz stereo float
    const std::string ff = ffmpeg();
    if (ff.empty()) { err = "Opening " + ext + " files needs ffmpeg on the PATH (WAV works without it)"; return false; }
#ifdef _WIN32
    const std::wstring cmd = L"\"\"" + fs::u8path(ff).wstring() + L"\" -v error -i \"" + fs::u8path(path).wstring() + L"\" -f f32le -ac 2 -ar 48000 -\"";
    FILE* p = _wpopen(cmd.c_str(), L"rb");
    if (!p) { err = "Can't run ffmpeg"; return false; }
    out.rate = 48000; out.channels = 2; out.samples.clear();
    std::vector<float> buf(65536);
    size_t n;
    while ((n = fread(buf.data(), sizeof(float), buf.size(), p)) > 0) out.samples.insert(out.samples.end(), buf.begin(), buf.begin() + n);
    _pclose(p);
#endif
    if (out.samples.empty()) { err = "ffmpeg couldn't read " + path; return false; }
    return true;
}

void overview(const Clip& c, int columns, std::vector<float>& lo, std::vector<float>& hi) {
    lo.assign(columns, 0); hi.assign(columns, 0);
    if (c.empty() || columns <= 0) return;
    const size_t frames = c.samples.size() / c.channels;
    for (int x = 0; x < columns; ++x) {
        const size_t a = frames * x / columns, b = std::max(a + 1, frames * (x + 1) / columns);
        float mn = 0, mx = 0;
        for (size_t f = a; f < b && f < frames; f += std::max<size_t>(1, (b - a) / 256))
            for (int ch = 0; ch < c.channels; ++ch) { const float v = c.samples[f * c.channels + ch]; mn = std::min(mn, v); mx = std::max(mx, v); }
        lo[x] = mn; hi[x] = mx;
    }
}

// ---------- analysis ----------
static void fft(std::vector<std::complex<float>>& a) {
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        const float ang = -2.0f * 3.14159265358979f / (float)len;
        const std::complex<float> wl(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len) {
            std::complex<float> w(1);
            for (size_t j = 0; j < len / 2; ++j) {
                const auto u = a[i + j], v = a[i + j + len / 2] * w;
                a[i + j] = u + v; a[i + j + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

Analysis analyse(const Clip& c) {
    Analysis r;
    if (c.empty()) return r;
    // mono, down to ~22 kHz
    const int step = std::max(1, c.rate / 22050);
    const double rate = (double)c.rate / step;
    std::vector<float> mono;
    const size_t frames = c.samples.size() / c.channels;
    mono.reserve(frames / step + 1);
    double sumSq = 0, peak = 0;
    for (size_t f = 0; f < frames; f += step) {
        float v = 0;
        for (int ch = 0; ch < c.channels; ++ch) v += c.samples[f * c.channels + ch];
        v /= c.channels;
        mono.push_back(v);
    }
    for (float v : c.samples) { sumSq += (double)v * v; peak = std::max(peak, (double)std::fabs(v)); }
    r.loudnessDb = 10 * std::log10(std::max(1e-12, sumSq / c.samples.size()));
    r.peakDb = 20 * std::log10(std::max(1e-9, peak));

    // the spectrum frame by frame: spectral flux for the beat, chroma for the key
    const int N = 4096, hop = 512;
    std::vector<float> win(N);
    for (int i = 0; i < N; ++i) win[i] = 0.5f - 0.5f * std::cos(2 * 3.14159265f * i / (N - 1));
    std::vector<float> prev(N / 2 + 1, 0), flux;
    double chroma[12] = {0};
    std::vector<std::complex<float>> buf(N);
    for (size_t start = 0; start + N <= mono.size(); start += hop) {
        for (int i = 0; i < N; ++i) buf[i] = mono[start + i] * win[i];
        fft(buf);
        double fl = 0;
        for (int k = 1; k <= N / 2; ++k) {
            const float m = std::log1p(100.0f * std::abs(buf[k]));
            fl += std::max(0.0f, m - prev[k]);
            prev[k] = m;
            const double hz = k * rate / N;
            if (hz < 55 || hz > 2000) continue;
            const double midi = 69 + 12 * std::log2(hz / 440.0);
            const int pc = ((int)std::lround(midi) % 12 + 12) % 12;
            chroma[pc] += std::abs(buf[k]);
        }
        flux.push_back((float)fl);
    }
    // tempo: the onset strength's autocorrelation, 60..200 BPM, leaning towards ~120 (as people tap)
    const double fps = rate / hop;
    if (flux.size() > 64) {
        double mean = 0;
        for (float v : flux) mean += v;
        mean /= flux.size();
        for (auto& v : flux) v = std::max(0.0f, v - (float)mean);
        double best = 0, bestLag = 0, total = 0;
        const int lo = (int)(fps * 60 / 200), hi = (int)(fps * 60 / 60);
        std::vector<double> ac(hi + 2, 0);
        for (int lag = lo; lag <= hi; ++lag) {
            double s = 0;
            for (size_t i = lag; i < flux.size(); ++i) s += flux[i] * flux[i - lag];
            ac[lag] = s;
        }
        for (int lag = lo; lag <= hi; ++lag) {
            const double bpm = 60 * fps / lag;
            const double prior = std::exp(-0.5 * std::pow(std::log2(bpm / 120.0) / 0.9, 2));
            const double score = ac[lag] * prior;
            total += score;
            if (score > best) { best = score; bestLag = lag; }
        }
        if (bestLag > 0) {
            // parabolic peak for a finer lag
            const int L = (int)bestLag;
            double lag = L;
            if (L > lo && L < hi) { const double a = ac[L - 1], b = ac[L], cc = ac[L + 1]; const double d = a - 2 * b + cc; if (std::fabs(d) > 1e-12) lag = L + 0.5 * (a - cc) / d; }
            r.bpm = std::round(60 * fps / lag * 10) / 10;
            r.bpmConfidence = total > 0 ? std::clamp(best / total * (hi - lo + 1) / 8.0, 0.0, 1.0) : 0;
        }
    }
    // key: Krumhansl-Schmuckler profiles against the chroma
    static const double major[12] = {6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88};
    static const double minor[12] = {6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17};
    static const char* names[12] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
    auto corr = [&](const double* prof, int tonic) {
        double mx = 0, my = 0;
        for (int i = 0; i < 12; ++i) { mx += chroma[i]; my += prof[i]; }
        mx /= 12; my /= 12;
        double sxy = 0, sxx = 0, syy = 0;
        for (int i = 0; i < 12; ++i) { const double x = chroma[(i + tonic) % 12] - mx, y = prof[i] - my; sxy += x * y; sxx += x * x; syy += y * y; }
        return sxx > 0 && syy > 0 ? sxy / std::sqrt(sxx * syy) : 0;
    };
    double best = -2, second = -2; std::string bestK, secondK;
    for (int t = 0; t < 12; ++t)
        for (int m = 0; m < 2; ++m) {
            const double v = corr(m ? minor : major, t);
            const std::string k = std::string(names[t]) + (m ? " minor" : " major");
            if (v > best) { second = best; secondK = bestK; best = v; bestK = k; }
            else if (v > second) { second = v; secondK = k; }
        }
    r.key = bestK; r.keyAlt = secondK;
    r.keyConfidence = std::clamp((best - second) * 5 + best * 0.5, 0.0, 1.0);
    return r;
}

}  // namespace Audio
