#include "TimingMap.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>

void TimingMap::sort() {
    std::stable_sort(points.begin(), points.end(),
        [](auto const& a, auto const& b) { return a.time < b.time; });
    std::stable_sort(rhythm.begin(), rhythm.end(),
        [](auto const& a, auto const& b) { return a.time < b.time; });
}

// ---------------- rhythm patterns ----------------

// a timing point up to this many ms after a rhythm point counts as being at the same place
static constexpr double RHYTHM_SLACK = 5.0;

int RhythmPoint::patternBeats() const {
    int n = 0;
    for (auto const& b : beats) n += std::max(1, b.span);
    return std::max(1, n);
}

// Timing point a rhythm point plays in, and where its beats are counted from
static TimingPoint const* rhythmTiming(TimingMap const& map, RhythmPoint const& rp, double& start) {
    start = rp.time;
    if (map.points.empty()) return nullptr;
    auto const& tp = map.points[std::max(0, map.indexAt(rp.time + RHYTHM_SLACK))];
    // starts together with that timing point -> count the beats exactly from it
    if (tp.time > start && tp.time - start <= RHYTHM_SLACK) start = tp.time;
    return tp.beatLength > 0 ? &tp : nullptr;
}

double TimingMap::rhythmEnd(int i) const {
    if (i < 0 || i >= (int)rhythm.size()) return 0;
    auto const& rp = rhythm[i];
    double start = rp.time;
    double end = 1e300;
    if (i + 1 < (int)rhythm.size()) end = rhythm[i + 1].time;
    // a BPM change ends the pattern (a timing point right at the start - a few ms off - belongs to it)
    for (auto const& p : points)
        if (p.time > start + RHYTHM_SLACK) { end = std::min(end, p.time); break; }
    // only a set number of repetitions
    double s;
    if (rp.loops > 0 && !rp.beats.empty())
        if (auto tp = rhythmTiming(*this, rp, s)) end = std::min(end, s + rp.loops * rp.patternBeats() * tp->beatLength);
    return end;
}

int TimingMap::rhythmAt(double t) const {
    int idx = -1;
    for (int i = 0; i < (int)rhythm.size(); i++) {
        if (rhythm[i].time <= t + 1e-6) idx = i;
        else break;
    }
    if (idx < 0 || rhythm[idx].beats.empty() || t >= rhythmEnd(idx) - 1e-3) return -1;
    return idx;
}

void TimingMap::forEachRhythmHit(double from, double to, std::function<void(double, TickKind)> const& cb) const {
    if (points.empty()) return;
    for (int i = 0; i < (int)rhythm.size(); i++) {
        auto const& rp = rhythm[i];
        if (rp.beats.empty()) continue;
        double start;
        auto tp = rhythmTiming(*this, rp, start);
        if (!tp) continue;
        double end = rhythmEnd(i);
        double lo = std::max(from, start), hi = std::min(to, end);
        if (hi < lo) continue;
        double bl = tp->beatLength;
        int meter = std::max(1, tp->meter);
        int len = rp.patternBeats();
        long long loop0 = std::max(0LL, (long long)std::floor((lo - start) / (len * bl) - 1e-9));
        for (long long loop = loop0;; loop++) {
            double loopStart = start + loop * len * bl;
            if (loopStart > hi + 1e-6) break;
            int beat = 0; // beats from the loop start
            for (auto const& b : rp.beats) {
                int span = std::max(1, b.span);
                int div = std::clamp(b.divisor, 1, 16);
                double stepStart = loopStart + beat * bl;
                for (int j = 0; j < div; j++) {
                    if (!(b.hits & (1u << j))) continue;
                    double t = stepStart + j * span * bl / div;
                    if (t < lo - 1e-6 || t > hi + 1e-6 || t >= end - 1e-3) continue;
                    TickKind kind = TickKind::Sub;
                    // accented hits are beats (downbeat when it is the first beat of a bar), the rest sub-beats
                    if (b.accents & (1u << j)) {
                        kind = TickKind::Beat;
                        if ((j * span) % div == 0) {
                            long long bt = std::llround((t - tp->time) / bl);
                            if (((bt % meter) + meter) % meter == 0) kind = TickKind::Downbeat;
                        }
                    }
                    cb(t, kind);
                }
                beat += span;
            }
        }
    }
}

void TimingMap::forEachClick(double from, double to, int divisor,
                             std::function<void(double, TickKind)> const& cb) const {
    forEachTick(from, to, divisor, [&](double t, TickKind k) {
        if (rhythm.empty() || rhythmAt(t) < 0) cb(t, k);
    });
    forEachRhythmHit(from, to, cb);
}

int TimingMap::indexAt(double t) const {
    if (points.empty()) return -1;
    int idx = 0;
    for (int i = 0; i < (int)points.size(); i++) {
        if (points[i].time <= t + 1e-6) idx = i;
        else break;
    }
    return idx;
}

double TimingMap::bpmAt(double t) const {
    int i = indexAt(t);
    return i < 0 ? 0.0 : points[i].bpm();
}

void TimingMap::forEachTick(double from, double to, int divisor,
                            std::function<void(double, TickKind)> const& cb) const {
    if (points.empty() || divisor < 1) return;
    for (size_t i = 0; i < points.size(); i++) {
        auto const& p = points[i];
        if (p.beatLength <= 0.0) continue;
        // pierwszy punkt rozciaga sie wstecz, ostatni do przodu
        double segStart = i == 0 ? std::min(from, p.time) : p.time;
        double segEnd = i + 1 < points.size() ? points[i + 1].time : std::max(to, p.time);
        double lo = std::max(segStart, from);
        double hi = std::min(segEnd, to);
        if (hi < lo) continue;

        double step = p.beatLength / divisor;
        long long k0 = (long long)std::ceil((lo - p.time) / step - 1e-6);
        for (long long k = k0;; k++) {
            double t = p.time + k * step;
            if (t > hi + 1e-6) break;
            // nie rysuj tickow tuz przed kolejnym punktem (osu tak robi)
            if (i + 1 < points.size() && t >= segEnd - 1e-3) break;
            TickKind kind = TickKind::Sub;
            if (k % divisor == 0) {
                long long beat = k / divisor;
                int meter = std::max(1, p.meter);
                kind = ((beat % meter) + meter) % meter == 0 ? TickKind::Downbeat : TickKind::Beat;
            }
            cb(t, kind);
        }
    }
}

double TimingMap::snap(double t, int divisor) const {
    int i = indexAt(t);
    if (i < 0) return t;
    auto const& p = points[i];
    double step = p.beatLength / std::max(1, divisor);
    double s = p.time + std::round((t - p.time) / step) * step;
    // nie przeskakuj za kolejny punkt
    if (i + 1 < (int)points.size() && s > points[i + 1].time) s = points[i + 1].time;
    return s;
}

GridPosition TimingMap::locate(double t, double tol) const {
    GridPosition g;
    int i = indexAt(t);
    if (i < 0) return g;
    // bars restart at every timing point (same as osu!)
    long long barsBefore = 0;
    for (int k = 0; k < i; k++) {
        auto const& p = points[k];
        double beats = (points[k + 1].time - p.time) / p.beatLength;
        barsBefore += (long long)std::ceil(beats / std::max(1, p.meter) - 1e-3);
    }
    auto const& p = points[i];
    int meter = std::max(1, p.meter);
    double beatsF = (t - p.time) / p.beatLength;
    long long beatIdx = (long long)std::floor(beatsF + 1e-6);
    double frac = beatsF - beatIdx;
    long long barInSeg = beatIdx >= 0 ? beatIdx / meter : -((-beatIdx + meter - 1) / meter);
    g.bar = int(barsBefore + barInSeg + 1);
    g.beat = int(((beatIdx % meter) + meter) % meter) + 1;
    g.bpm = p.bpm();
    g.valid = true;
    static int const divs[] = { 1, 2, 3, 4, 6, 8, 12, 16 };
    for (int d : divs) {
        double n = std::round(frac * d);
        double err = (frac - n / d) * p.beatLength;
        if (std::abs(err) <= tol) {
            g.den = d;
            g.num = (int)n;
            g.errorMs = err;
            if (g.num == d) { g.num = 0; g.den = 1; } // rounded up to the next beat
            // reduce, e.g. 2/4 -> 1/2
            while (g.num % 2 == 0 && g.den % 2 == 0 && g.num > 0) { g.num /= 2; g.den /= 2; }
            if (g.num == 0) g.den = 1;
            return g;
        }
    }
    // off grid: report distance to the nearest 1/16
    g.den = 0;
    g.num = 0;
    g.errorMs = (frac - std::round(frac * 16) / 16) * p.beatLength;
    return g;
}

std::string GridPosition::describe() const {
    if (!valid) return "no timing";
    char buf[160];
    char const* names[] = { "", "1/1 (beat)", "1/2", "1/3", "1/4", "", "1/6", "", "1/8", "", "", "", "1/12", "", "", "", "1/16" };
    if (den == 0) {
        std::snprintf(buf, sizeof buf, "Bar %d  Beat %d  -  OFF GRID (%+.1f ms from 1/16)", bar, beat, errorMs);
    } else if (num == 0) {
        std::snprintf(buf, sizeof buf, "Bar %d  Beat %d  -  on %s%s  (%+.1f ms)", bar, beat,
            beat == 1 ? "downbeat, " : "", names[1], errorMs);
    } else {
        std::snprintf(buf, sizeof buf, "Bar %d  Beat %d + %d/%d  -  on %s grid  (%+.1f ms)", bar, beat, num, den,
            names[den], errorMs);
    }
    return buf;
}

static std::string trim(std::string s) {
    while (!s.empty() && (s.back() == '\r' || s.back() == ' ' || s.back() == '\t')) s.pop_back();
    size_t b = 0;
    while (b < s.size() && (s[b] == ' ' || s[b] == '\t' || (unsigned char)s[b] == 0xEF ||
                            (unsigned char)s[b] == 0xBB || (unsigned char)s[b] == 0xBF)) b++;
    return s.substr(b);
}

TimingMap TimingMap::fromOsu(std::string const& text, std::string* audioFilename) {
    TimingMap map;
    std::istringstream in(text);
    std::string line;
    std::string section;
    bool sawSection = false;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line.rfind("//", 0) == 0) continue;
        if (line.front() == '[') {
            section = line;
            sawSection = true;
            continue;
        }
        if (section == "[General]" && audioFilename && line.rfind("AudioFilename:", 0) == 0) {
            *audioFilename = trim(line.substr(14));
            continue;
        }
        if (sawSection && section != "[TimingPoints]") continue;

        // time,beatLength,meter,sampleSet,sampleIndex,volume,uninherited,effects
        std::vector<std::string> f;
        std::stringstream ls(line);
        std::string tok;
        while (std::getline(ls, tok, ',')) f.push_back(tok);
        if (f.size() < 2) continue;
        char* end = nullptr;
        double time = std::strtod(f[0].c_str(), &end);
        if (end == f[0].c_str()) continue;
        double beatLength = std::strtod(f[1].c_str(), nullptr);
        bool uninherited = f.size() >= 7 ? std::atoi(f[6].c_str()) == 1 : beatLength > 0;
        if (!uninherited || beatLength <= 0) continue;
        TimingPoint p;
        p.time = time;
        p.beatLength = beatLength;
        p.meter = f.size() >= 3 ? std::max(1, std::atoi(f[2].c_str())) : 4;
        map.points.push_back(p);
    }
    map.sort();
    return map;
}

std::string TimingMap::toOsuSection() const {
    std::string out = "[TimingPoints]\n";
    char buf[128];
    for (auto const& p : points) {
        std::snprintf(buf, sizeof buf, "%.0f,%.12g,%d,1,0,100,1,0\n", p.time, p.beatLength, p.meter);
        out += buf;
    }
    return out;
}

std::string TimingMap::serialize() const {
    std::string out;
    char buf[128];
    for (auto const& p : points) {
        std::snprintf(buf, sizeof buf, "%.4f,%.12g,%d;", p.time, p.beatLength, p.meter);
        out += buf;
    }
    // rhythm patterns: |Rtime,bars,div:hits/div:hits/...;
    if (!rhythm.empty()) {
        out += "|R";
        for (auto const& r : rhythm) {
            std::snprintf(buf, sizeof buf, "%.4f,%d,L%d,", r.time, r.bars, r.loops);
            out += buf;
            for (auto const& b : r.beats) {
                std::snprintf(buf, sizeof buf, "%d:%u:%d:%u/", b.divisor, b.hits, b.span, b.accents);
                out += buf;
            }
            out += ";";
        }
    }
    return out;
}

TimingMap TimingMap::deserialize(std::string const& full) {
    TimingMap map;
    auto rpos = full.find("|R");
    std::string s = full.substr(0, rpos);
    if (rpos != std::string::npos) {
        std::stringstream rs(full.substr(rpos + 2));
        std::string item;
        while (std::getline(rs, item, ';')) {
            RhythmPoint r;
            int bars = 1;
            char beatsBuf[2048] = {};
            int loops = 0;
            // newer: time,bars,Lloops,beats   older: time,bars,beats
            int n = std::sscanf(item.c_str(), "%lf,%d,L%d,%2047s", &r.time, &bars, &loops, beatsBuf);
            if (n < 3) n = std::sscanf(item.c_str(), "%lf,%d,%2047s", &r.time, &bars, beatsBuf);
            if (n < 2) continue;
            r.bars = std::clamp(bars, 1, 4);
            r.loops = std::max(0, loops);
            std::stringstream bs(beatsBuf);
            std::string beat;
            while (std::getline(bs, beat, '/')) {
                RhythmBeat b;
                int n = std::sscanf(beat.c_str(), "%d:%u:%d:%u", &b.divisor, &b.hits, &b.span, &b.accents);
                if (n >= 2) {
                    b.divisor = std::clamp(b.divisor, 1, 16);
                    b.span = std::clamp(b.span, 1, 16);
                    // saved before accents existed -> the hits on beat lines are accented
                    if (n < 4) b.accents = defaultAccents(b.divisor, b.span);
                    r.beats.push_back(b);
                }
            }
            map.rhythm.push_back(r);
        }
    }
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ';')) {
        TimingPoint p;
        int meter = 4;
        if (std::sscanf(item.c_str(), "%lf,%lf,%d", &p.time, &p.beatLength, &meter) >= 2 && p.beatLength > 0) {
            p.meter = std::max(1, meter);
            map.points.push_back(p);
        }
    }
    map.sort();
    return map;
}
