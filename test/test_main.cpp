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
    testAnalyzer();
    std::printf(failures ? "\n%d FAILURE(S)\n" : "\nALL OK\n", failures);
    return failures ? 1 : 0;
}
