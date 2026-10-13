#include "Triggers.hpp"
#include "analysis/Analyzer.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/LevelEditorLayer.hpp>
#include <Geode/binding/LevelSettingsObject.hpp>
#include <Geode/binding/UndoObject.hpp>

#include <algorithm>
#include <cmath>
#include <map>

using namespace geode::prelude;

namespace triggers {

namespace {
char const* const kEveryNames[kEveryCount] = { "BAR", "1/1", "1/2", "1/3", "1/4", "1/6", "1/8", "HITS" };
int const kEveryDivs[kEveryCount] = { 0, 1, 2, 3, 4, 6, 8, -1 };

constexpr int PULSE_ID = 1006, SPAWN_ID = 1268, STOP_ID = 1616, COLOR_ID = 899;

// Hits of the song: peaks of the onset envelope that stand out from their surroundings (a few seconds)
std::vector<double> songHits(OnsetEnvelope const& env, double from, double to) {
    constexpr double STEP = 2.0, BLOCK = 500.0;
    std::vector<double> out;
    if (to <= from) return out;
    // loudest hit of every half second -> the level of the music around a moment
    std::vector<float> blocks;
    for (double b = from; b < to; b += BLOCK) {
        float m = 0;
        for (double t = b; t < std::min(to, b + BLOCK); t += STEP) m = std::max(m, env.at(t));
        blocks.push_back(m);
    }
    auto around = [&](double t) {
        int i = (int)((t - from) / BLOCK);
        float m = 0;
        for (int j = std::max(0, i - 3); j <= std::min((int)blocks.size() - 1, i + 3); j++) m = std::max(m, blocks[j]);
        return m;
    };
    double lastT = -1e18;
    float lastV = 0;
    for (double t = from + STEP; t < to; t += STEP) {
        float v = env.at(t);
        if (v < std::max(.1f, around(t) * .45f) || v < env.at(t - 4) || v < env.at(t + 4)) continue;
        // closer than 70 ms = the same hit (keep the stronger peak)
        if (t - lastT < 70) {
            if (v > lastV) { out.back() = t; lastT = t; lastV = v; }
            continue;
        }
        out.push_back(t);
        lastT = t;
        lastV = v;
    }
    return out;
}

std::string num(double v) { return fmt::format("{:.4f}", v); }

// A stretch of the times that one spawn loop can play: the same BPM and the same rhythm pattern (or the regular
// grid). It repeats every `period` ms; offsets = the moments inside one repetition, from its first time.
struct Segment {
    size_t first = 0, last = 0;
    double period = 0;
    std::vector<double> offsets;
};
std::vector<Segment> loopSegments(TimingMap const& map, Settings const& s, std::vector<double> const& ts) {
    std::vector<Segment> out;
    int div = everyDivisor(s.every);
    if (div < 0 || map.empty()) return out;
    auto key = [&](double t) {
        int pi = std::max(0, map.indexAt(t + .5));
        int ri = div > 0 ? map.rhythmAt(t + .5) : -1;
        if (ri >= 0 && map.rhythm[ri].beats.empty()) ri = -1; // a "normal" point = the regular grid
        return std::pair{ pi, ri };
    };
    size_t i = 0;
    while (i < ts.size()) {
        auto k = key(ts[i]);
        size_t j = i;
        while (j + 1 < ts.size() && key(ts[j + 1]) == k) j++;
        auto const& p = map.points[k.first];
        Segment seg{ i, j };
        if (k.second >= 0) seg.period = std::max(1, map.rhythm[k.second].patternBeats()) * p.beatLength;
        else seg.period = div == 0 ? p.beatLength * std::max(1, p.meter) : p.beatLength / div;
        for (size_t n = i; n <= j && ts[n] - ts[i] < seg.period - .5; n++) seg.offsets.push_back(ts[n] - ts[i]);
        out.push_back(std::move(seg));
        i = j + 1;
    }
    return out;
}

struct PaletteColor {
    char const* name;
    unsigned char r, g, b;
};
// 0 = brighten the channel's own color (HSV), the last one = no pulse
PaletteColor const kPalette[] = {
    { "BRIGHT", 255, 255, 255 }, { "WHITE", 255, 255, 255 }, { "PINK", 255, 90, 200 }, { "PURPLE", 160, 80, 255 },
    { "BLUE", 60, 140, 255 },    { "CYAN", 60, 230, 255 },   { "GREEN", 80, 255, 100 }, { "YELLOW", 255, 230, 60 },
    { "RED", 255, 60, 60 },      { "OFF", 40, 40, 50 },
};
constexpr int kPaletteCount = (int)(sizeof(kPalette) / sizeof(kPalette[0]));

struct Target {
    char const* name;
    int channel;
};
// special color channels of GD: 1000 = BG, 1001 = Ground 1, 1002 = Line, 1004 = Obj
Target const kTargets[] = { { "BG", 1000 }, { "GROUND", 1001 }, { "OBJECTS", 1004 }, { "LINE", 1002 } };

struct Preset {
    char const* name;
    int steps;
    int colors[kMaxSteps];
};
Preset const kPresets[] = {
    { "EVERY BEAT", 1, { 0 } },
    { "TWO COLORS", 2, { 2, 4 } },        // pink, blue
    { "ALTERNATE", 2, { 0, 9 } },         // flash, nothing
    { "ACCENT", 4, { 1, 3, 3, 3 } },      // white then purple
    { "RAINBOW", 4, { 8, 7, 6, 4 } },     // red, yellow, green, blue
};
constexpr int kPresetCount = (int)(sizeof(kPresets) / sizeof(kPresets[0]));

int gcd(int a, int b) { return b ? gcd(b, a % b) : a; }
} // namespace

int paletteCount() { return kPaletteCount; }
char const* paletteName(int c) { return kPalette[std::clamp(c, 0, kPaletteCount - 1)].name; }
void paletteRGB(int c, unsigned char& r, unsigned char& g, unsigned char& b) {
    auto const& p = kPalette[std::clamp(c, 0, kPaletteCount - 1)];
    r = p.r;
    g = p.g;
    b = p.b;
}
bool paletteIsOff(int c) { return c >= kPaletteCount - 1; }

void stepRGB(Step const& st, unsigned char& r, unsigned char& g, unsigned char& b) {
    if (st.color == 1) {
        r = (st.rgb >> 16) & 255;
        g = (st.rgb >> 8) & 255;
        b = st.rgb & 255;
    } else paletteRGB(st.color, r, g, b);
}

int targetCount() { return (int)(sizeof(kTargets) / sizeof(kTargets[0])); }
char const* targetName(int t) { return kTargets[std::clamp(t, 0, targetCount() - 1)].name; }
int targetChannel(int t) { return kTargets[std::clamp(t, 0, targetCount() - 1)].channel; }

int presetCount() { return kPresetCount; }
char const* presetName(int p) { return kPresets[std::clamp(p, 0, kPresetCount - 1)].name; }
void applyPreset(Settings& s, int p) {
    auto const& pr = kPresets[std::clamp(p, 0, kPresetCount - 1)];
    s.steps = pr.steps;
    for (int i = 0; i < kMaxSteps; i++) {
        s.step[i].color = i < pr.steps ? pr.colors[i] : 0;
        // spawn: the steps that are "off" in the preset spawn nothing, the others keep the groups
        if (i < pr.steps && paletteIsOff(pr.colors[i])) s.step[i].groups.clear();
        else if (s.step[i].groups.empty()) s.step[i].groups = "1";
    }
}

std::vector<int> parseGroups(std::string const& text) {
    std::vector<int> out;
    int cur = -1;
    for (char c : text + ",") {
        if (c >= '0' && c <= '9') cur = (cur < 0 ? 0 : cur * 10) + (c - '0');
        else {
            if (cur > 0 && cur < 10000) out.push_back(cur);
            cur = -1;
        }
    }
    return out;
}

bool Settings::active(int i) const {
    i = std::clamp(i, 0, kMaxSteps - 1);
    return kind != Kind::Spawn ? !paletteIsOff(step[i].color) : !parseGroups(step[i].groups).empty();
}

char const* everyName(int every) { return kEveryNames[std::clamp(every, 0, kEveryCount - 1)]; }
int everyDivisor(int every) { return kEveryDivs[std::clamp(every, 0, kEveryCount - 1)]; }

std::pair<double, double> range(Settings const& s, double songEnd) {
    double from = s.fromMs < 0 ? 0 : s.fromMs;
    double to = s.toMs < 0 ? songEnd : s.toMs;
    if (to < from) std::swap(from, to);
    return { from, to };
}

std::vector<double> times(TimingMap const& map, OnsetEnvelope const* env, Settings const& s, double songEnd) {
    std::vector<double> out;
    auto [from, to] = range(s, songEnd);
    int div = everyDivisor(s.every);
    if (div < 0) {
        if (env) out = songHits(*env, from, to);
        return out;
    }
    if (map.empty()) return out;
    auto add = [&](double t, TickKind k) {
        if (t < from - .5 || t > to + .5) return;
        if (div == 0 && k != TickKind::Downbeat) return;
        out.push_back(t);
    };
    // inside rhythm patterns their hits (like the metronome and the guidelines)
    if (div > 0) map.forEachClick(from, to, div, add);
    else map.forEachTick(from, to, std::max(1, div), add);
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end(), [](double a, double b) { return std::abs(a - b) < .5; }), out.end());
    return out;
}

int objectCount(Settings const& s, size_t n) {
    int steps = std::clamp(s.steps, 1, kMaxSteps), total = 0;
    for (size_t i = 0; i < n; i++) {
        int st = (int)(i % steps);
        total += s.kind != Kind::Spawn ? (s.active(st) ? 1 : 0) : (int)parseGroups(s.step[st].groups).size();
    }
    return total;
}

int loopCount(TimingMap const& map, Settings const& s, std::vector<double> const& ts) {
    return (int)loopSegments(map, s, ts).size();
}

int place(TimingMap const& map, OnsetEnvelope const* env, Settings const& s, double songEnd, std::string* error) {
    auto fail = [&](std::string msg) {
        if (error) *error = std::move(msg);
        return 0;
    };
    auto lel = LevelEditorLayer::get();
    if (!lel || !lel->m_levelSettings) return fail("Open this from the level editor");
    bool loop = s.placement == Placement::Loop;
    int K = std::clamp(s.steps, 1, kMaxSteps);
    if (loop && everyDivisor(s.every) < 0) return fail("A spawn loop needs a steady beat (BAR or 1/N), not HITS");
    bool any = false;
    for (int i = 0; i < K; i++) any = any || s.active(i);
    if (!any) return fail(s.kind != Kind::Spawn ? "All the steps are OFF (pick a color for a step)"
                                                 : "No group to spawn (type a group into a step)");
    if (everyDivisor(s.every) >= 0 && map.empty()) return fail("Add a timing point (BPM) first");

    auto ts = times(map, env, s, songEnd);
    double offsetSec = lel->m_levelSettings->m_songOffset;
    // song time -> position in the level (speed portals included); the moments before the song start are skipped
    ts.erase(std::remove_if(ts.begin(), ts.end(), [&](double t) { return t / 1000.0 - offsetSec < 0; }), ts.end());
    if (ts.empty()) return fail("No beats in this part");
    auto xAt = [&](double t) { return lel->posForTime(float(t / 1000.0 - offsetSec)).x; };

    int layer = std::max<int>(0, lel->m_currentLayer);
    // the height of the middle of the editor view (where the person using it is), on the block grid
    auto win = CCDirector::get()->getWinSize();
    float y = std::floor(lel->m_objectLayer->convertToNodeSpace(win / 2).y / 30.f) * 30.f + 15.f;
    auto head = [&](int id, float x, float yy) {
        return fmt::format("1,{},2,{:.2f},3,{:.0f},20,{},36,1", id, x, yy, layer);
    };
    // spawned by the loop instead of by the player
    auto inGroup = [](int group) { return fmt::format(",62,1,87,1,57,{}", group); };
    int channel = targetChannel(s.target);
    // a pulse of this color: "BRIGHT" brightens the channel's own color (HSV, tested in a real level),
    // the others tint it with the color
    auto pulse = [&](Step const& stp, float x, float yy, std::string const& extra) {
        int color = stp.color;
        if (s.kind == Kind::Color) {
            // a color trigger: the channel gets that color right away (static); keys: 7 8 9 = rgb, 10 = duration,
            // 23 = the color channel, 35 = opacity
            unsigned char r, g, b;
            stepRGB(stp, r, g, b);
            return head(COLOR_ID, x, yy) + fmt::format(",7,{},8,{},9,{},10,0,23,{},35,1", r, g, b, channel) + extra;
        }
        auto o = head(PULSE_ID, x, yy);
        if (color == 0)
            o += fmt::format(",51,{},48,1,49,0a1a0.45a0a1,50,{},45,0,46,0.03,47,0.3", channel, channel);
        else {
            unsigned char r, g, b;
            stepRGB(stp, r, g, b);
            o += fmt::format(",7,{},8,{},9,{},51,{},52,0,45,0,46,0.03,47,0.3", r, g, b, channel);
        }
        return o + extra;
    };
    auto spawn = [&](float x, float yy, int target, double delay, std::string const& extra) {
        return head(SPAWN_ID, x, yy) + fmt::format(",51,{},63,{}", target, num(delay)) + extra;
    };

    std::vector<std::string> objs;
    if (!loop) {
        for (size_t i = 0; i < ts.size(); i++) {
            int st = (int)(i % K);
            if (!s.active(st)) continue;
            float x = xAt(ts[i]);
            if (s.kind != Kind::Spawn) objs.push_back(pulse(s.step[st], x, y, ""));
            else {
                float yy = y;
                for (int g : parseGroups(s.step[st].groups)) {
                    objs.push_back(spawn(x, yy, g, 0, ""));
                    yy += 30;
                }
            }
        }
    } else {
        // one loop per BPM and per rhythm pattern: a start trigger on the first time, the loop (spawns itself
        // every repetition) in its own group, and a stop trigger after the last time of that stretch / of the part.
        // The loop is as long as it takes for the steps and the hits to line up again; every hit spawns its step
        // (a pulse in its own group, or the groups you typed) delayed to the hit.
        gd::unordered_set<int> used;
        auto freeGroup = [&] {
            int g = lel->getNextFreeGroupID(used);
            if (g > 0) used.insert(g);
            return g;
        };
        for (auto const& seg : loopSegments(map, s, ts)) {
            int m = (int)seg.offsets.size();
            int r = K / gcd(m, K);
            double period = seg.period * r;
            int group = freeGroup();
            if (group <= 0) return fail("No free group left for the loop");
            float x0 = xAt(ts[seg.first]);
            objs.push_back(spawn(x0, y, group, 0, ""));
            objs.push_back(spawn(x0, y + 30, group, period / 1000.0, inGroup(group)));
            std::map<int, int> colorGroup; // pulse color -> group holding that pulse
            float row = y + 60, actionRow = y + 60 + 30.f * m * r;
            for (int a = 0; a < r; a++)
                for (int b = 0; b < m; b++) {
                    int idx = a * m + b, st = idx % K;
                    if (!s.active(st)) continue;
                    double delay = (seg.offsets[b] + a * seg.period) / 1000.0;
                    float x = x0 + 8.f * (idx % 6);
                    if (s.kind != Kind::Spawn) {
                        int color = s.step[st].color == 1 ? int(0x1000000 | s.step[st].rgb) : s.step[st].color;
                        auto it = colorGroup.find(color);
                        if (it == colorGroup.end()) {
                            int g = freeGroup();
                            if (g <= 0) return fail("No free group left for the loop");
                            it = colorGroup.emplace(color, g).first;
                            objs.push_back(pulse(s.step[st], x0, actionRow, inGroup(g)));
                            actionRow += 30;
                        }
                        objs.push_back(spawn(x, row, it->second, delay, inGroup(group)));
                        row += 30;
                    } else {
                        for (int g : parseGroups(s.step[st].groups)) {
                            objs.push_back(spawn(x, row, g, delay, inGroup(group)));
                            row += 30;
                        }
                    }
                }
            // half of the shortest gap after the last time: it still plays, the next one never comes
            double gap = seg.period;
            for (size_t k = 0; k < seg.offsets.size(); k++) {
                double next = k + 1 < seg.offsets.size() ? seg.offsets[k + 1] : seg.period + seg.offsets[0];
                gap = std::min(gap, next - seg.offsets[k]);
            }
            double stopT = ts[seg.last] + std::max(1.0, gap / 2);
            objs.push_back(head(STOP_ID, xAt(stopT), y) + fmt::format(",51,{}", group));
        }
    }

    std::string str;
    for (auto const& o : objs) {
        if (!str.empty()) str += ";";
        str += o;
    }
    auto arr = lel->createObjectsFromString(str, true, true);
    if (!arr || arr->count() == 0) return fail("The editor did not add the triggers");
    // all of them = one step of the editor's undo (Ctrl+Z removes them together)
    lel->addToUndoList(UndoObject::createWithArray(arr, UndoCommand::Paste), false);
    log::info("Placed {} trigger(s)", arr->count());
    return (int)arr->count();
}

} // namespace triggers
