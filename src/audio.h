// Hymnal - audio: WAV in and out, anything else through ffmpeg, a waveform overview, and the song's key and tempo
// measured from the sound itself (no model needed).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace Audio {

struct Clip {
    int rate = 48000, channels = 2;
    std::vector<float> samples;        // interleaved, -1..1
    double seconds() const { return channels > 0 && rate > 0 ? (double)samples.size() / channels / rate : 0; }
    bool empty() const { return samples.empty(); }
};

bool readWav(const std::string& data, Clip& out, std::string& err);            // bytes of a .wav
bool load(const std::string& path, Clip& out, std::string& err);               // .wav itself; others through ffmpeg
bool saveWav(const std::string& path, const Clip& c, int bits, std::string& err);   // 16, 24 or 32 (float)
std::string wavBytes(const Clip& c, int bits);
std::string ffmpeg();                                                          // "" when it isn't on this PC

// min/max per column, for drawing
void overview(const Clip& c, int columns, std::vector<float>& lo, std::vector<float>& hi);

struct Analysis {
    double bpm = 0, bpmConfidence = 0;
    std::string key;                    // "F# minor"
    double keyConfidence = 0;
    std::string keyAlt;                 // the runner-up (often the relative major/minor)
    double loudnessDb = 0;              // RMS, dBFS
    double peakDb = 0;
};
Analysis analyse(const Clip& c);

}  // namespace Audio
