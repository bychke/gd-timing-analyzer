#pragma once
#include <Geode/Result.hpp>

#include <filesystem>
#include <string>
#include <vector>

// Zdekodowany plik audio (mono) + peaki do rysowania waveformu.
struct AudioData {
    std::vector<float> mono;
    int sampleRate = 44100;
    double lengthMs = 0;

    // min/max dla blokow po BLOCK probek
    static constexpr int BLOCK = 64;
    std::vector<float> peakMin, peakMax;

    void buildPeaks();
    // min/max w przedziale czasu [a, b) ms
    std::pair<float, float> range(double aMs, double bMs) const;
};

// Dekoduje dowolny format obslugiwany przez FMOD (mp3/ogg/wav/flac...). Bezpieczne w watku tla.
geode::Result<AudioData> decodeAudio(std::filesystem::path const& path);
