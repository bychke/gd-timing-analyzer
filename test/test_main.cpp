// Testy czesci niezaleznych od gry: parser .osu, siatka beatow, analiza BPM na syntetycznym nagraniu.
// Budowanie: patrz test/build.bat
#include "../src/analysis/Analyzer.hpp"
#include "../src/timing/TimingMap.hpp"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <random>
#include <sstream>

static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { failures++; std::printf("FAIL: " __VA_ARGS__); std::printf("\n"); } } while (0)

static void testOsu(char const* path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { std::printf("skip osu test (no file %s)\n", path); return; }
    std::stringstream ss; ss << f.rdbuf();
    std::string audio;
    auto map = TimingMap::fromOsu(ss.str(), &audio);
    std::printf("osu: %zu uninherited points, audio=%s\n", map.points.size(), audio.c_str());
    CHECK(audio == "audio.mp3", "audio filename");
    CHECK(map.points.size() > 10, "point count");
    CHECK(map.points[0].time == 45, "first time %f", map.points[0].time);
    CHECK(std::abs(map.points[0].bpm() - 190.0) < 0.01, "first bpm %f", map.points[0].bpm());
    CHECK(std::abs(map.points[1].bpm() - 191.0) < 0.01, "second bpm %f", map.points[1].bpm());
    for (size_t i = 0; i < map.points.size() && i < 6; i++)
        std::printf("  %8.0f ms  %7.3f BPM  %d/4\n", map.points[i].time, map.points[i].bpm(), map.points[i].meter);

    auto round = TimingMap::deserialize(map.serialize());
    CHECK(round.points.size() == map.points.size(), "serialize roundtrip");
    auto again = TimingMap::fromOsu(map.toOsuSection());
    CHECK(again.points.size() == map.points.size(), "osu roundtrip");
}

static void testRhythm() {
    TimingMap m;
    m.points.push_back({ 0, 600, 4 });      // 100 BPM
    m.points.push_back({ 4800, 500, 4 });   // BPM change after 2 bars
    RhythmPoint r;
    r.time = 1200;                          // beat 3 of bar 1
    r.beats = { { 1, 1 }, { 6, 0b1111 } };  // beat A: one hit, beat B: 4 hits on the 1/6 grid
    m.rhythm.push_back(r);
    RhythmPoint normal;
    normal.time = 4200;                     // pattern stops here
    m.rhythm.push_back(normal);

    std::vector<double> hits;
    m.forEachRhythmHit(0, 10000, [&](double t, TickKind) { hits.push_back(t); });
    // beats at 1200 (A), 1800 (B x4), 2400 (A), 3000 (B x4), 3600 (A) -> stops at 4200
    CHECK(hits.size() == 1 + 4 + 1 + 4 + 1, "rhythm hit count %zu", hits.size());
    CHECK(std::abs(hits[1] - 1800) < 1e-6 && std::abs(hits[2] - 1900) < 1e-6 && std::abs(hits[4] - 2100) < 1e-6,
        "1/6 hits");
    CHECK(m.rhythmAt(1000) == -1 && m.rhythmAt(2000) == 0 && m.rhythmAt(4300) == -1, "rhythmAt");

    // regular 1/1 grid outside the pattern, pattern inside
    int clicks = 0;
    m.forEachClick(0, 4799, 1, [&](double, TickKind) { clicks++; });
    // regular beats 0, 600 + 4200 = 3, pattern 11
    CHECK(clicks == 3 + 11, "click count %d", clicks);

    // a BPM change ends a pattern even without a Normal point
    m.rhythm.pop_back();
    CHECK(std::abs(m.rhythmEnd(0) - 4800) < 1e-6, "end at BPM change");

    auto round = TimingMap::deserialize(m.serialize());
    CHECK(round.rhythm.size() == 1 && round.rhythm[0].beats.size() == 2 && round.rhythm[0].beats[1].divisor == 6 &&
          round.rhythm[0].beats[1].hits == 0b1111 && round.points.size() == 2, "rhythm serialize roundtrip");
    // a rhythm point a fraction of a ms before a timing point belongs to that point (not ended by it)
    TimingMap m2;
    m2.points.push_back({ 0, 600, 4 });
    m2.points.push_back({ 1000.4, 500, 4 });
    RhythmPoint r2;
    r2.time = 1000;
    r2.beats = { { 2, 0b11 } };
    m2.rhythm.push_back(r2);
    int n2 = 0;
    m2.forEachRhythmHit(0, 1999, [&](double, TickKind) { n2++; });
    CHECK(n2 == 4, "rhythm next to a timing point: %d hits", n2);

    // a step over 2 beats with 1/3 = three even hits over two beats; loops limit the repeats
    TimingMap m3;
    m3.points.push_back({ 0, 600, 4 });
    RhythmPoint r3;
    r3.time = 0;
    r3.loops = 2;
    r3.beats = { { 1, 1, 1 }, { 1, 1, 1 }, { 3, 0b111, 2 } };   // 4 beats = 1 bar
    m3.rhythm.push_back(r3);
    std::vector<double> h3;
    m3.forEachRhythmHit(0, 100000, [&](double t, TickKind) { h3.push_back(t); });
    CHECK(h3.size() == 2 * 5, "span/loops hit count %zu", h3.size());
    CHECK(h3.size() >= 5 && std::abs(h3[2] - 1200) < 1e-6 && std::abs(h3[3] - 1600) < 1e-6 && std::abs(h3[4] - 2000) < 1e-6,
        "triplet over two beats");
    CHECK(std::abs(m3.rhythmEnd(0) - 4800) < 1e-6 && m3.rhythmAt(5000) == -1, "loops end");
    auto r3b = TimingMap::deserialize(m3.serialize());
    CHECK(r3b.rhythm.size() == 1 && r3b.rhythm[0].loops == 2 && r3b.rhythm[0].beats[2].span == 2, "span/loops roundtrip");
    auto oldRhythm = TimingMap::deserialize("0.0000,600,4;|R0.0000,1,2:3/;");
    CHECK(oldRhythm.rhythm.size() == 1 && oldRhythm.rhythm[0].beats[0].divisor == 2 && oldRhythm.rhythm[0].beats[0].span == 1,
        "older rhythm format");

    auto old = TimingMap::deserialize("0.0000,600,4;");
    CHECK(old.points.size() == 1 && old.rhythm.empty(), "old format");
}

static void testTicks() {
    TimingMap m;
    m.points.push_back({ 1000, 500, 4 });
    m.points.push_back({ 3100, 400, 3 });
    int down = 0, beat = 0, sub = 0;
    m.forEachTick(0, 4000, 2, [&](double t, TickKind k) {
        if (k == TickKind::Downbeat) down++; else if (k == TickKind::Beat) beat++; else sub++;
    });
    std::printf("ticks: down=%d beat=%d sub=%d\n", down, beat, sub);
    CHECK(std::abs(m.snap(1260, 1) - 1500) < 1e-6, "snap");
    CHECK(std::abs(m.snap(3350, 2) - 3300) < 1e-6, "snap seg2");
    CHECK(m.indexAt(3200) == 1, "indexAt");

    auto g = m.locate(1000 + 500 + 500 * 3 / 8.0); // bar 1, beat 2, 3/8
    std::printf("locate: %s\n", g.describe().c_str());
    CHECK(g.bar == 1 && g.beat == 2 && g.num == 3 && g.den == 8, "locate 3/8");
    g = m.locate(1000 + 250);
    CHECK(g.bar == 1 && g.beat == 1 && g.num == 1 && g.den == 2, "locate 1/2");
    g = m.locate(1000 + 500 * 4);
    CHECK(g.bar == 2 && g.beat == 1 && g.num == 0, "locate downbeat");
    g = m.locate(1000 + 37);
    CHECK(g.den == 0, "off grid");
    g = m.locate(3100 + 400 * 3); // second point, meter 3 -> next bar
    std::printf("locate seg2: %s\n", g.describe().c_str());
    CHECK(g.beat == 1 && g.bar == 4, "bar count across points (got bar %d)", g.bar);
}

static void testAnalyzer() {
    // syntetyczny "utwor": 44.1 kHz, 15 s @ 190 BPM, potem 15 s @ 172 BPM, start 1.3 s
    int sr = 44100;
    std::vector<float> audio(sr * 32, 0.f);
    std::mt19937 rng(1);
    std::normal_distribution<float> noise(0, 0.01f);
    for (auto& s : audio) s = noise(rng);
    auto hit = [&](double ms, float amp) {
        size_t s0 = (size_t)(ms / 1000.0 * sr);
        for (int i = 0; i < sr / 20 && s0 + i < audio.size(); i++)
            audio[s0 + i] += amp * std::exp(-i / (sr * 0.01f)) * std::sin(i * 0.3f + (i % 7) * 0.9f);
    };
    double t = 1300, L1 = 60000.0 / 190, L2 = 60000.0 / 172;
    int k = 0;
    for (; t < 16300; t += L1, k++) hit(t, k % 4 == 0 ? 0.9f : 0.5f);
    double change = t;
    for (; t < 31000; t += L2, k++) hit(t, k % 4 == 0 ? 0.9f : 0.5f);

    auto env = computeOnsetEnvelope(audio, sr);
    auto res = analyzeTiming(env, {});
    std::printf("analysis: first beat %.1f ms, %zu points\n", res.firstBeatMs, res.map.points.size());
    for (auto& p : res.map.points) std::printf("  %8.1f ms  %7.3f BPM\n", p.time, p.bpm());
    CHECK(!res.map.empty(), "has points");
    if (res.map.empty()) return;
    CHECK(std::abs(res.map.points[0].bpm() - 190) < 0.5, "bpm1 %f", res.map.points[0].bpm());
    CHECK(std::abs(res.firstBeatMs - 1300) < 10, "first beat %f", res.firstBeatMs);
    CHECK(std::abs(res.map.points[0].time - 1300) < 10, "first downbeat %f", res.map.points[0].time);
    CHECK(std::abs(res.map.bpmAt(25000) - 172) < 0.5, "bpm2 %f", res.map.bpmAt(25000));
    // beat tuz po zmianie musi lezec na siatce (+-10 ms)
    double after = change + L2 * 8;
    double snapped = res.map.snap(after, 1);
    CHECK(std::abs(snapped - after) < 10, "grid after change: %f vs %f", snapped, after);
}

int main(int argc, char** argv) {
    testOsu(argc > 1 ? argv[1] : "Children of Bodom - You're Better Off Dead (Mazzerin) [LMT's Expert].osu");
    testTicks();
    testRhythm();
    testAnalyzer();
    std::printf(failures ? "\n%d FAILURE(S)\n" : "\nALL OK\n", failures);
    return failures ? 1 : 0;
}
