// SPDX-License-Identifier: Apache-2.0
// Owned C ABI for the isolated Python worker. No C++ layouts cross ctypes.
#include "tts_service.h"
#include "text/fst_normalizer.hpp"
#include "text/text_normalizer.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr std::size_t kMaxAudio = 32 * 1024 * 1024;
struct Engine { std::unique_ptr<SpacemiT::TtsEngine> tts; };
using Cancel = int32_t (*)();

std::vector<std::string> clauses(const std::string& text) {
    std::vector<std::string> out;
    std::string current;
    std::size_t count = 0;
    for (std::size_t i = 0; i < text.size();) {
        const auto first = static_cast<unsigned char>(text[i]);
        const std::size_t size = first < 0x80 ? 1 : first < 0xe0 ? 2 : first < 0xf0 ? 3 : 4;
        if (i + size > text.size()) throw std::runtime_error("Invalid UTF-8");
        const auto ch = text.substr(i, size);
        current += ch;
        i += size;
        ++count;
        const bool punctuation = ch == "。" || ch == "！" || ch == "？" || ch == "；" ||
                                 ch == "." || ch == "!" || ch == "?" || ch == ";" || ch == "\n";
        // Do not normally split an English word at the soft character budget.
        const bool word = i < text.size() && first < 0x80 &&
            ((first >= 'A' && first <= 'Z') || (first >= 'a' && first <= 'z')) &&
            static_cast<unsigned char>(text[i]) < 0x80 && text[i] != ' ';
        if (punctuation || (count >= 48 && !word) || count >= 96) {
            if (current.find_first_not_of(" \t\r\n") != std::string::npos) out.push_back(current);
            current.clear(); count = 0;
        }
    }
    if (current.find_first_not_of(" \t\r\n") != std::string::npos) out.push_back(current);
    return out;
}

void put16(uint8_t* p, uint16_t n) { p[0] = n & 255; p[1] = (n >> 8) & 255; }
void put32(uint8_t* p, uint32_t n) { for (int i=0; i<4; ++i) p[i] = (n >> (8*i)) & 255; }
}

extern "C" {
int32_t rv_tts_abi_version() { return 1; }

void* rv_tts_create_matcha(const char* model_dir, int32_t threads) noexcept {
    try {
        const char* root = std::getenv("RVAI_TTS_CACHE_ROOT");
        if (!root || !*root || !model_dir || threads < 1 || threads > 8) return nullptr;
        namespace fs = std::filesystem;
        if (!fs::path(root).is_absolute() || fs::weakly_canonical(model_dir) !=
            fs::weakly_canonical(fs::path(root) / "models/tts/matcha-tts")) return nullptr;
        const auto rules = fs::path(root) / "models/tts/text_norm/v1/zh";
        tts::text::FstNormalizer check;
        if (!check.Load({(rules/"date.fst").string(), (rules/"phone.fst").string(),
                         (rules/"number.fst").string()})) return nullptr;
        const auto value = tts::text::normalizeText("音量50%。", tts::text::Language::ZH);
        if (value.find("百分之五十") == std::string::npos) return nullptr;
        setenv("SPACEMIT_TTS_CPU_THREADS", std::to_string(threads).c_str(), 1);
        setenv("RVAI_TTS_NO_SPIN", "1", 1);
        auto config = SpacemiT::TtsConfig::Preset("matcha_zh_en");
        config.provider = "cpu";
        config.model_dir = model_dir;
        config.num_threads = threads;
        config.enable_warmup = true;
        config.speech_rate = 1.0f;
        config.lexicon.push_back({"KylinBot", "key lin bot", "en"});
        auto result = std::make_unique<Engine>();
        result->tts = std::make_unique<SpacemiT::TtsEngine>(config);
        if (!result->tts->IsInitialized() || result->tts->GetSampleRate() != 16000 ||
            result->tts->GetNumSpeakers() != 1) return nullptr;
        return result.release();
    } catch (...) { return nullptr; }
}

int32_t rv_tts_sample_rate(void* handle) { return handle ? 16000 : 0; }
int32_t rv_tts_num_speakers(void* handle) { return handle ? 1 : 0; }
void rv_tts_free_audio(void* p) { std::free(p); }
void rv_tts_destroy(void* handle) { delete static_cast<Engine*>(handle); }

// Diagnostic function used by fixed regression tests, not exposed over HTTP.
void* rv_tts_normalize_text(const char* text) noexcept {
    try {
        const auto out = tts::text::normalizeText(text ? text : "", tts::text::Language::ZH);
        auto* result = std::malloc(out.size()+1);
        if (result) std::memcpy(result, out.c_str(), out.size()+1);
        return result;
    } catch (...) { return nullptr; }
}

int32_t rv_tts_generate_wav_cancelable(void* handle, const char* text, int32_t sid,
    float speed, void** buffer, int64_t* size, Cancel cancel) noexcept {
    if (!buffer || !size) return 0;
    *buffer = nullptr; *size = 0;
    try {
        if (!handle || !text || sid != 0 || !std::isfinite(speed) || speed < 0.5f || speed > 2.f) return 0;
        auto& engine = *static_cast<Engine*>(handle);
        const auto normalized = tts::text::normalizeText(text, tts::text::Language::ZH);
        if (normalized.empty() || normalized.size() > 65536) return 0;
        const auto parts = clauses(normalized);
        std::vector<uint8_t> pcm;
        engine.tts->SetSpeed(speed);
        for (const auto& part : parts) {
            if (cancel && cancel()) return 2;
            auto audio = engine.tts->Call(part);
            if (cancel && cancel()) return 2;
            if (!audio || !audio->IsSuccess() || audio->IsEmpty() || audio->GetSampleRate() != 16000) return 0;
            const auto data = audio->GetAudioData();
            if (data.empty() || pcm.size() + data.size() > kMaxAudio - 44) return 0;
            pcm.insert(pcm.end(), data.begin(), data.end());
        }
        if (pcm.empty()) return 0;
        auto* wav = static_cast<uint8_t*>(std::malloc(pcm.size()+44));
        if (!wav) return 0;
        std::memcpy(wav, "RIFF", 4); put32(wav+4, pcm.size()+36);
        std::memcpy(wav+8, "WAVEfmt ", 8); put32(wav+16, 16);
        put16(wav+20, 1); put16(wav+22, 1); put32(wav+24, 16000);
        put32(wav+28, 32000); put16(wav+32, 2); put16(wav+34, 16);
        std::memcpy(wav+36, "data", 4); put32(wav+40, pcm.size());
        std::memcpy(wav+44, pcm.data(), pcm.size());
        *buffer = wav; *size = pcm.size()+44;
        return 1;
    } catch (...) { return 0; }
}

int32_t rv_tts_generate_wav(void* handle, const char* text, int32_t sid,
    float speed, void** buffer, int64_t* size) noexcept {
    return rv_tts_generate_wav_cancelable(handle, text, sid, speed, buffer, size, nullptr);
}
}
