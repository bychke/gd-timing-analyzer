#include "Metronome.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/FMODAudioEngine.hpp>
#include <fmod.hpp>

#include <cmath>
#include <vector>

using namespace geode::prelude;

namespace {
FMOD::Sound* g_hi = nullptr;
FMOD::Sound* g_lo = nullptr;
std::vector<int16_t> g_bufHi, g_bufLo;

// short decaying sine "tick" as raw PCM16
FMOD::Sound* makeClick(FMOD::System* sys, std::vector<int16_t>& buf, float freq) {
    constexpr int SR = 44100, LEN = SR / 25;
    buf.resize(LEN);
    for (int i = 0; i < LEN; i++) {
        float env = std::exp(-i / (SR * 0.006f));
        buf[i] = int16_t(std::sin(2 * 3.14159265f * freq * i / SR) * env * 26000);
    }
    FMOD_CREATESOUNDEXINFO ex{};
    ex.cbsize = sizeof(ex);
    ex.length = LEN * sizeof(int16_t);
    ex.numchannels = 1;
    ex.defaultfrequency = SR;
    ex.format = FMOD_SOUND_FORMAT_PCM16;
    FMOD::Sound* s = nullptr;
    sys->createSound((char const*)buf.data(), FMOD_OPENMEMORY | FMOD_OPENRAW | FMOD_CREATESAMPLE, &ex, &s);
    return s;
}
}

void metronome::click(bool downbeat) {
    auto sys = FMODAudioEngine::get()->m_system;
    if (!sys) return;
    if (!g_hi) {
        g_hi = makeClick(sys, g_bufHi, 1600);
        g_lo = makeClick(sys, g_bufLo, 1000);
    }
    auto snd = downbeat ? g_hi : g_lo;
    if (!snd) return;
    FMOD::Channel* ch = nullptr;
    sys->playSound(snd, nullptr, true, &ch);
    if (ch) {
        ch->setVolume(float(Mod::get()->getSettingValue<int64_t>("metronome-volume")) / 100.f);
        ch->setPaused(false);
    }
}

void metronome::release() {
    if (g_hi) g_hi->release();
    if (g_lo) g_lo->release();
    g_hi = g_lo = nullptr;
}

void metronome::Tracker::update(double posMs, TimingMap const& map) {
    // the setting moves clicks earlier (+) or later (-) to compensate audio latency
    double pos = posMs + double(Mod::get()->getSettingValue<int64_t>("metronome-offset"));
    if (map.empty() || pos <= m_last || pos - m_last > 300) {
        m_last = pos;
        return;
    }
    bool hit = false, down = false;
    map.forEachTick(m_last + 0.001, pos, 1, [&](double, TickKind k) {
        hit = true;
        down = down || k == TickKind::Downbeat;
    });
    if (hit) click(down);
    m_last = pos;
}
