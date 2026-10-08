#include "AudioData.hpp"

#include <Geode/Geode.hpp>
#include <fmod.hpp>

#include <algorithm>
#include <cstring>

using namespace geode::prelude;

void AudioData::buildPeaks() {
    size_t blocks = (mono.size() + BLOCK - 1) / BLOCK;
    peakMin.assign(blocks, 0.f);
    peakMax.assign(blocks, 0.f);
    for (size_t b = 0; b < blocks; b++) {
        float lo = 0, hi = 0;
        size_t end = std::min(mono.size(), (b + 1) * BLOCK);
        for (size_t i = b * BLOCK; i < end; i++) {
            lo = std::min(lo, mono[i]);
            hi = std::max(hi, mono[i]);
        }
        peakMin[b] = lo;
        peakMax[b] = hi;
    }
}

std::pair<float, float> AudioData::range(double aMs, double bMs) const {
    if (peakMin.empty()) return { 0.f, 0.f };
    long long a = (long long)(aMs / 1000.0 * sampleRate / BLOCK);
    long long b = (long long)(bMs / 1000.0 * sampleRate / BLOCK);
    a = std::clamp<long long>(a, 0, (long long)peakMin.size() - 1);
    b = std::clamp<long long>(b, a + 1, (long long)peakMin.size());
    float lo = 0, hi = 0;
    for (long long i = a; i < b; i++) {
        lo = std::min(lo, peakMin[i]);
        hi = std::max(hi, peakMax[i]);
    }
    return { lo, hi };
}

Result<AudioData> decodeAudio(std::filesystem::path const& path) {
    // Separate FMOD system (no output) - does not touch the game's music and works on any thread.
    FMOD::System* sys = nullptr;
    if (FMOD::System_Create(&sys) != FMOD_OK || !sys) return Err("FMOD::System_Create failed");
    sys->setOutput(FMOD_OUTPUTTYPE_NOSOUND_NRT);
    if (sys->init(1, FMOD_INIT_NORMAL, nullptr) != FMOD_OK) {
        sys->release();
        return Err("FMOD init failed");
    }
    auto cleanup = [&](FMOD::Sound* s) { if (s) s->release(); sys->close(); sys->release(); };

    FMOD::Sound* sound = nullptr;
    auto u8 = geode::utils::string::pathToString(path);
    if (sys->createSound(u8.c_str(), FMOD_OPENONLY | FMOD_ACCURATETIME | FMOD_CREATESTREAM, nullptr, &sound) != FMOD_OK || !sound) {
        cleanup(nullptr);
        return Err("Cannot open audio file: {}", u8);
    }

    FMOD_SOUND_FORMAT format;
    int channels = 0, bits = 0;
    float freq = 44100;
    sound->getFormat(nullptr, &format, &channels, &bits);
    sound->getDefaults(&freq, nullptr);
    unsigned int lenPcm = 0;
    sound->getLength(&lenPcm, FMOD_TIMEUNIT_PCM);
    if (channels <= 0 || lenPcm == 0) {
        cleanup(sound);
        return Err("Empty or unsupported audio file");
    }

    AudioData data;
    data.sampleRate = (int)freq;
    data.mono.reserve(lenPcm);

    int bytesPerSample = bits / 8;
    std::vector<unsigned char> buf(1 << 16);
    size_t frameBytes = (size_t)bytesPerSample * channels;
    std::vector<unsigned char> carry;
    while (true) {
        unsigned int read = 0;
        auto r = sound->readData(buf.data(), (unsigned int)buf.size(), &read);
        if (read == 0) break;
        carry.insert(carry.end(), buf.begin(), buf.begin() + read);
        size_t frames = carry.size() / frameBytes;
        for (size_t f = 0; f < frames; f++) {
            float sum = 0;
            for (int c = 0; c < channels; c++) {
                unsigned char const* p = carry.data() + f * frameBytes + c * bytesPerSample;
                float v = 0;
                switch (format) {
                    case FMOD_SOUND_FORMAT_PCM8: v = ((int)*p - 128) / 128.f; break;
                    case FMOD_SOUND_FORMAT_PCM16: { int16_t s; std::memcpy(&s, p, 2); v = s / 32768.f; break; }
                    case FMOD_SOUND_FORMAT_PCM24: { int32_t s = (p[0] << 8) | (p[1] << 16) | (p[2] << 24); v = (s >> 8) / 8388608.f; break; }
                    case FMOD_SOUND_FORMAT_PCM32: { int32_t s; std::memcpy(&s, p, 4); v = s / 2147483648.f; break; }
                    case FMOD_SOUND_FORMAT_PCMFLOAT: std::memcpy(&v, p, 4); break;
                    default: break;
                }
                sum += v;
            }
            data.mono.push_back(sum / channels);
        }
        carry.erase(carry.begin(), carry.begin() + frames * frameBytes);
        if (r != FMOD_OK) break;
    }
    cleanup(sound);

    if (data.mono.empty()) return Err("Failed to decode audio");
    data.lengthMs = data.mono.size() * 1000.0 / data.sampleRate;
    data.buildPeaks();
    return Ok(std::move(data));
}
