#pragma once
#include "timing/TimingMap.hpp"

#include <string>
#include <vector>

struct OnsetEnvelope;

// Pulse / spawn triggers placed in the level editor on the beats of the timing (Triggers tab)
namespace triggers {

// Pulse = a pulse trigger, Spawn = a spawn trigger, Color = a color trigger (sets a color channel to a color)
enum class Kind { Pulse = 0, Spawn = 1, Color = 2 };
enum class Placement { OnBeats = 0, Loop = 1 };

// "Every": once per bar, 1/1 ... 1/8 of a beat, or on the hits found in the song
inline constexpr int kEveryCount = 8;
char const* everyName(int every);     // "BAR", "1/1" ... "1/8", "HITS"
int everyDivisor(int every);          // 0 = bar, -1 = hits

// Colors of a pulse step: 0 = brighten the channel's own color, then fixed colors, the last = no pulse
int paletteCount();
char const* paletteName(int color);
void paletteRGB(int color, unsigned char& r, unsigned char& g, unsigned char& b);
bool paletteIsOff(int color);
struct Step;

// The color of a step as RGB (a picked color, or the ready one)
void stepRGB(Step const& step, unsigned char& r, unsigned char& g, unsigned char& b);

// What the pulse flashes
int targetCount();
char const* targetName(int target);
int targetChannel(int target);

inline constexpr int kMaxSteps = 8;

// One step of the repeating cycle. The triggers get the steps one after another (the 1st, 2nd ... Nth, then again):
// a pulse step has a color (or none), a spawn step spawns its groups ("1,5,12"; empty = nothing)
struct Step {
    int color = 0;              // 0 = brighten the channel's own color, 1 = the color `rgb`, 2..8 = ready colors, 9 = none
    unsigned rgb = 0xFFFFFF;    // color 1: 0xRRGGBB
    std::string groups = "1";
};

struct Settings {
    Kind kind = Kind::Pulse;
    Placement placement = Placement::OnBeats;
    int every = 1;              // index, see everyName
    double fromMs = -1;         // song time; < 0 = from the start
    double toMs = -1;           // < 0 = to the end
    int target = 0;             // pulse: see targetName
    int steps = 1;              // how many steps the cycle has
    Step step[kMaxSteps];

    bool active(int i) const;   // does step i make a trigger at all
};

// Ready cycles to start from
int presetCount();
char const* presetName(int preset);
void applyPreset(Settings& s, int preset);

// "1, 5;12" -> 1, 5, 12
std::vector<int> parseGroups(std::string const& text);

// Song times (ms) of the triggers in the part (sorted). songEnd = length of the song.
std::vector<double> times(TimingMap const& map, OnsetEnvelope const* env, Settings const& s, double songEnd);

// How many trigger objects the times make on the beats (not counting loops)
int objectCount(Settings const& s, size_t times);

// How many spawn loops the times need: one per BPM, one per rhythm pattern (and per stretch of the regular grid)
int loopCount(TimingMap const& map, Settings const& s, std::vector<double> const& ts);

// The part as song times (from/to resolved)
std::pair<double, double> range(Settings const& s, double songEnd);

// Puts the triggers into the open level editor as one undo step, at the height the editor view is at. Returns how many
// objects were added (0 = nothing / error, see error).
int place(TimingMap const& map, OnsetEnvelope const* env, Settings const& s, double songEnd, std::string* error);

} // namespace triggers
