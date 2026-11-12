// Hymnal - the engine: acestep.cpp's ace-server (ACE-Step 1.5 in C++ on ggml, MIT), run by Hymnal on a private port
// with the models folder, closed with Hymnal. Jobs go in as JSON (and audio), come back as songs, stems or analysis.
#pragma once
#include "http.h"
#include "json.h"
#include <string>
#include <vector>

namespace Engine {

std::string serverExe();                           // "" when it isn't found
std::string modelsDir();                           // F:\AIModels\music when F:\AIModels exists, else %APPDATA%\Atelier\models\music
void setModelsDir(const std::string& dir);
std::string logPath();

bool start(std::string& err);                      // (re)starts it if it isn't answering
bool running();                                    // /health answers
void stop();
std::string url();                                 // http://127.0.0.1:<port>

// the models it found: lm, embedding, dit, vae
struct Models { std::vector<std::string> lm, embedding, dit, vae; };
bool models(Models& m, std::string& err);

enum Status { S_QUEUED, S_RUNNING, S_DONE, S_FAILED, S_CANCELLED, S_UNKNOWN };
bool submit(const char* endpoint, const std::string& json, std::string& id, std::string& err);   // /lm or /synth
bool submitAudio(const char* endpoint, const std::string& json, const std::string& audio, const std::string& audioName, std::string& id, std::string& err,
                 const std::string& refAudio = "");                                            // /synth, /understand
Status poll(const std::string& id);
Http::Response result(const std::string& id);
void cancel(const std::string& id);

}  // namespace Engine
