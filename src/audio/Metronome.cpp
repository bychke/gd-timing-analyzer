#include "Metronome.hpp"
#include "../Options.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/FMODAudioEngine.hpp>
#include <fmod.hpp>

#include <algorithm>
#include <cmath>
#include <map>
#include <random>
#include <vector>

using namespace geode::prelude;

namespace {
constexpr int SR = 44100;
constexpr float PI = 3.14159265f;

// the two clicks of one sound type (high = first beat of a bar)
struct ClickPair {
    FMOD::Sound* hi = nullptr;
    FMOD::Sound* lo = nullptr;
};
std::map<std::string, ClickPair> g_sounds;

// Synthesizes one click of the given type as float samples in [-1, 1]
std::vector<float> synth(std::string const& type, bool hi) {
    std::vector<float> out;
    std::mt19937 rng(hi ? 7 : 3);
    std::uniform_real_distribution<float> noise(-1.f, 1.f);
    auto gen = [&](float seconds, auto&& fn) {
        int n = int(SR * seconds);
        out.resize(n);
        for (int i = 0; i < n; i++) out[i] = fn(float(i) / SR);
    };
    if (type == "wood") {
        // hollow knock: low sine + a tiny noise attack
        float f = hi ? 1250.f : 850.f;
        gen(.06f, [&](float t) {
            return std::sin(2 * PI * f * t) * std::exp(-t / .012f) * .9f + noise(rng) * std::exp(-t / .0015f) * .35f;
        });
    } else if (type == "click") {
        // very short dry tick (brighter on the first beat)
        float decay = hi ? .0008f : .0012f;
        float prev = 0;
        gen(.02f, [&](float t) {
            float v = noise(rng) * std::exp(-t / decay);
            float r = hi ? v - prev * .6f : v * .6f + prev * .4f; // high-pass / low-pass
            prev = v;
            return r * 1.4f;
        });
    } else {
        // classic: short decaying sine
        float f = hi ? 1600.f : 1000.f;
        gen(1.f / 25, [&](float t) { return std::sin(2 * PI * f * t) * std::exp(-t / .006f) * .8f; });
    }
    return out;
}

FMOD::Sound* makeSound(FMOD::System* sys, std::vector<float> const& samples) {
    std::vector<int16_t> buf(samples.size());
    for (size_t i = 0; i < samples.size(); i++)
        buf[i] = int16_t(std::clamp(samples[i], -1.f, 1.f) * 30000);
    FMOD_CREATESOUNDEXINFO ex{};
    ex.cbsize = sizeof(ex);
    ex.length = unsigned(buf.size() * sizeof(int16_t));
    ex.numchannels = 1;
    ex.defaultfrequency = SR;
    ex.format = FMOD_SOUND_FORMAT_PCM16;
    FMOD::Sound* s = nullptr;
    // FMOD_OPENMEMORY copies the data, the buffer can go away
    sys->createSound((char const*)buf.data(), FMOD_OPENMEMORY | FMOD_OPENRAW | FMOD_CREATESAMPLE, &ex, &s);
    return s;
}
}

int metronome::ticksPerBeat() {
    auto t = opt::get<std::string>("metronome-ticks");
    return t == "1/2" ? 2 : t == "1/3" ? 3 : t == "1/4" ? 4 : 1;
}

void metronome::click(TickKind kind) {
    auto sys = FMODAudioEngine::get()->m_system;
    if (!sys) return;
    auto type = opt::get<std::string>("metronome-sound");
    auto& pair = g_sounds[type];
    if (!pair.hi) {
        pair.hi = makeSound(sys, synth(type, true));
        pair.lo = makeSound(sys, synth(type, false));
    }
    auto snd = kind == TickKind::Downbeat ? pair.hi : pair.lo;
    if (!snd) return;
    float vol = float(opt::get<int64_t>("metronome-volume")) / 100.f;
    if (kind == TickKind::Downbeat) vol *= float(opt::get<int64_t>("metronome-accent")) / 100.f;
    else if (kind == TickKind::Sub) vol *= .45f;
    FMOD::Channel* ch = nullptr;
    sys->playSound(snd, nullptr, true, &ch);
    if (ch) {
        ch->setVolume(vol);
        ch->setPaused(false);
    }
}

void metronome::release() {
    for (auto& [_, p] : g_sounds) {
        if (p.hi) p.hi->release();
        if (p.lo) p.lo->release();
    }
    g_sounds.clear();
}

void metronome::Tracker::update(double posMs, TimingMap const& map) {
    // the setting moves clicks earlier (+) or later (-) to compensate audio latency
    double pos = posMs + double(Mod::get()->getSettingValue<int64_t>("metronome-offset"));
    if (map.empty() || pos <= m_last || pos - m_last > 300) {
        m_last = pos;
        return;
    }
    // strongest line passed this frame (regular grid or rhythm pattern): Downbeat > Beat > Sub
    int best = -1;
    map.forEachClick(m_last + 0.001, pos, ticksPerBeat(), [&](double, TickKind k) {
        int v = k == TickKind::Downbeat ? 2 : k == TickKind::Beat ? 1 : 0;
        best = std::max(best, v);
    });
    if (best >= 0) click(best == 2 ? TickKind::Downbeat : best == 1 ? TickKind::Beat : TickKind::Sub);
    m_last = pos;
}
