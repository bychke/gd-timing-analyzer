#pragma once
// Czysty C++ (bez Geode): wykrywanie BPM, zmian tempa i poczatku beatu.
#include "../timing/TimingMap.hpp"

#include <atomic>
#include <vector>

struct OnsetEnvelope {
    std::vector<float> values; // sila onsetu na klatke (znormalizowana 0..1)
    double framesPerSecond = 0.0;

    float at(double ms) const; // interpolacja liniowa
};

struct AnalyzerSettings {
    double minBpm = 70.0;
    double maxBpm = 230.0;
    double preferredBpm = 150.0;     // srodek "preferencji" przy wyborze oktawy
    double changeTolerance = 0.012;  // wzgledna roznica BPM uznawana za zmiane tempa
    double minSegmentSeconds = 6.0;  // krotsze odcinki sa scalane z sasiadami
    int meter = 4;
    bool detectChanges = true;       // false = one constant BPM for the whole song
};

struct AnalysisResult {
    TimingMap map;
    double firstBeatMs = 0.0; // gdzie zaczyna sie beat
};

OnsetEnvelope computeOnsetEnvelope(std::vector<float> const& mono, int sampleRate,
                                   std::atomic<float>* progress = nullptr);

AnalysisResult analyzeTiming(OnsetEnvelope const& env, AnalyzerSettings const& settings,
                             std::atomic<float>* progress = nullptr);

// Szuka najsilniejszego onsetu w oknie +-windowMs wokol t (do snapowania offsetu)
double nearestOnset(OnsetEnvelope const& env, double t, double windowMs);
