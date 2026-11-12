// Hymnal - the ACE-Step 1.5 models it can fetch (Hugging Face: Serveurperso/ACE-Step-1.5-GGUF), with what each is for
#pragma once
#include <string>
#include <vector>

namespace Catalog {

enum Role { LM, SONG, TEXT, VAE };

struct Model {
    const char* file;
    Role role;
    int mb;
    const char* about;
    bool starter;      // in the set a 6 GB card runs well
};

inline const std::vector<Model>& models() {
    static const std::vector<Model> m = {
        // the songwriter: lyrics, structure and the song's "codes"
        {"acestep-5Hz-lm-0.6B-Q8_0.gguf", LM, 677, "Songwriter, small: quickest, simplest songs", false},
        {"acestep-5Hz-lm-1.7B-Q8_0.gguf", LM, 1884, "Songwriter, medium: the usual choice", true},
        {"acestep-5Hz-lm-4B-Q5_K_M.gguf", LM, 2886, "Songwriter, large (Q5): better songs and lyrics", false},
        {"acestep-5Hz-lm-4B-Q8_0.gguf", LM, 4251, "Songwriter, large (Q8): the best, needs ~6 GB", false},
        // the song model: turns the codes into sound
        {"acestep-v15-turbo-Q6_K.gguf", SONG, 1879, "Singer and band, turbo (8 steps): fast", true},
        {"acestep-v15-turbo-Q8_0.gguf", SONG, 2431, "Singer and band, turbo (Q8): fast, a little finer", false},
        {"acestep-v15-sft-Q6_K.gguf", SONG, 1879, "Singer and band, SFT (50 steps): slower, more detail", false},
        {"acestep-v15-base-Q6_K.gguf", SONG, 1879, "Base: needed for STEMS (extract), add-a-track (lego) and complete", false},
        {"acestep-v15-base-Q4_K_M.gguf", SONG, 1379, "Base (smaller): stems, lego, complete", false},
        {"acestep-v15-turbo-shift3-Q6_K.gguf", SONG, 1879, "Turbo, shift 3: another flavour of turbo", false},
        {"acestep-v15-xl-turbo-Q4_K_M.gguf", SONG, 2851, "XL turbo (4B, Q4): bigger and richer, heavier", false},
        {"acestep-v15-xl-sft-Q4_K_M.gguf", SONG, 2851, "XL SFT (4B, Q4): the richest, slow", false},
        // always needed
        {"Qwen3-Embedding-0.6B-Q8_0.gguf", TEXT, 748, "Text encoder: reads your style prompt (needed)", true},
        {"vae-BF16.gguf", VAE, 322, "Audio decoder (needed)", true},
    };
    return m;
}

inline std::string url(const char* file) { return std::string("https://huggingface.co/Serveurperso/ACE-Step-1.5-GGUF/resolve/main/") + file; }
inline const char* roleName(Role r) { static const char* n[] = {"Songwriter", "Song model", "Text encoder", "Audio decoder"}; return n[r]; }

}  // namespace Catalog
