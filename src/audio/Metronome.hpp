#pragma once
#include "../timing/TimingMap.hpp"

#include <string>

namespace metronome {
    // Plays a single click: Downbeat = first beat of a bar (higher + louder), Beat, Sub = between beats (quieter).
    // Sound, ticks and accent come from the mod settings.
    void click(TickKind kind);
    // Frees the click sounds (on mod unload / FMOD reset).
    void release();
    // Ticks per beat from the "metronome-ticks" setting (1, 2, 3, 4)
    int ticksPerBeat();

    // Follows a playing song and clicks on every beat (or sub-beat) line it passes.
    class Tracker {
    public:
        // posMs = current song position. Large jumps (seek) reset without clicking.
        void update(double posMs, TimingMap const& map);
        void reset() { m_last = -1e18; }

    private:
        double m_last = -1e18;
    };
}
