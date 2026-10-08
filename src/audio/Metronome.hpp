#pragma once
#include "../timing/TimingMap.hpp"

namespace metronome {
    // Plays a single click (higher pitch on the first beat of a bar).
    void click(bool downbeat);
    // Frees the click sounds (on mod unload / FMOD reset).
    void release();

    // Follows a playing song and clicks on every beat line it passes.
    class Tracker {
    public:
        // posMs = current song position. Large jumps (seek) reset without clicking.
        void update(double posMs, TimingMap const& map);
        void reset() { m_last = -1e18; }

    private:
        double m_last = -1e18;
    };
}
