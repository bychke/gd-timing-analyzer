#pragma once
// Czysty C++ (bez Geode) - testowalny poza gra.
#include <functional>
#include <string>
#include <vector>

// Odpowiednik "uninherited timing point" z osu!
struct TimingPoint {
    double time = 0.0;        // ms od poczatku pliku audio
    double beatLength = 500.0; // ms na jeden beat (60000 / BPM)
    int meter = 4;            // beatow w takcie

    double bpm() const { return 60000.0 / beatLength; }
    void setBpm(double bpm) { beatLength = 60000.0 / bpm; }
};

enum class TickKind { Downbeat, Beat, Sub };

// One step of a rhythm pattern: `span` beats split into `divisor` parts, bit j of `hits` = a hit on part j
// (span 2 + divisor 3 = three even hits over two beats). Bit j of `accents` = that hit is accented
// (pink: louder metronome click, beat-colored guideline); other hits are quiet sub-beats.
struct RhythmBeat {
    int divisor = 1;
    unsigned hits = 1;
    int span = 1;
    unsigned accents = 1;
};

// Default accents: the parts that fall on a beat line
inline unsigned defaultAccents(int divisor, int span) {
    unsigned a = 0;
    for (int j = 0; j < divisor && j < 32; j++)
        if ((j * span) % divisor == 0) a |= 1u << j;
    return a;
}

// Rhythm pattern (1, 2 or 4 bars) that repeats from `time` until the next rhythm point or BPM change,
// or only `loops` times (0 = no limit). Empty `beats` = "Normal" point (back to the regular grid).
struct RhythmPoint {
    double time = 0.0;
    int bars = 1;
    int loops = 0;
    std::vector<RhythmBeat> beats;

    // length of one repetition in beats
    int patternBeats() const;
};

// Where a moment in time lies on the beat grid
struct GridPosition {
    bool valid = false;
    int bar = 0;        // 1-based, counted from the first timing point
    int beat = 0;       // 1-based beat inside the bar
    int num = 0;        // position inside the beat: num/den (0 = exactly on the beat)
    int den = 1;        // finest snap the time sits on: 1, 2, 3, 4, 6, 8, 12, 16 (0 = off grid)
    double errorMs = 0; // distance to that grid line
    double bpm = 0;

    std::string describe() const;
};

class TimingMap {
public:
    std::vector<TimingPoint> points;
    std::vector<RhythmPoint> rhythm;

    bool empty() const { return points.empty(); }
    void sort();

    // --- rhythm patterns ---
    // Where rhythm point i stops (next rhythm point or the next BPM change)
    double rhythmEnd(int i) const;
    // Rhythm point whose pattern plays at t (-1 = regular grid)
    int rhythmAt(double t) const;
    // Every hit of the rhythm patterns in [from, to]
    void forEachRhythmHit(double from, double to, std::function<void(double, TickKind)> const& cb) const;
    // Regular grid (divisor) outside patterns + pattern hits inside them (metronome, guidelines)
    void forEachClick(double from, double to, int divisor, std::function<void(double, TickKind)> const& cb) const;
    // indeks timing pointa obowiazujacego w czasie t (dla t < pierwszy -> 0), -1 gdy pusto
    int indexAt(double t) const;
    double bpmAt(double t) const;

    // Wywoluje cb dla kazdej linii siatki w [from, to]. divisor: 1, 2, 4, 3, 6...
    void forEachTick(double from, double to, int divisor,
                     std::function<void(double, TickKind)> const& cb) const;

    // Najblizsza linia siatki do t
    double snap(double t, int divisor) const;

    // Bar / beat / snap divisor of time t. toleranceMs = how close counts as "on the grid".
    GridPosition locate(double t, double toleranceMs = 2.0) const;

    // --- osu! ---
    // Parsuje caly plik .osu (albo sama sekcje [TimingPoints]). Bierze tylko uninherited.
    static TimingMap fromOsu(std::string const& text, std::string* audioFilename = nullptr);
    std::string toOsuSection() const;

    // --- zapis wlasny: "time,beatLength,meter;..." ---
    std::string serialize() const;
    static TimingMap deserialize(std::string const& s);
};
