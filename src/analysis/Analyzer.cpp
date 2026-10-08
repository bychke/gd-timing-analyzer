#include "Analyzer.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <numeric>

namespace {

constexpr double PI = 3.14159265358979323846;
constexpr int FFT_SIZE = 1024;
constexpr double TARGET_FPS = 200.0; // rozdzielczosc obwiedni onsetow (5 ms)

void fft(std::vector<std::complex<float>>& a) {
    size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; i++) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        double ang = -2 * PI / len;
        std::complex<float> wl((float)std::cos(ang), (float)std::sin(ang));
        for (size_t i = 0; i < n; i += len) {
            std::complex<float> w(1);
            for (size_t j = 0; j < len / 2; j++) {
                auto u = a[i + j], v = a[i + j + len / 2] * w;
                a[i + j] = u + v;
                a[i + j + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

// wartosc obwiedni w (ulamkowej) klatce
float envAt(std::vector<float> const& e, double f) {
    if (f < 0 || f >= (double)e.size() - 1) return 0.f;
    size_t i = (size_t)f;
    float t = (float)(f - i);
    return e[i] * (1 - t) + e[i + 1] * t;
}

// autokorelacja obwiedni na (ulamkowym) opoznieniu lag w oknie [s, s+w)
double acf(std::vector<float> const& e, size_t s, size_t w, double lag) {
    if (lag < 1) return 0;
    double sum = 0;
    size_t cnt = 0;
    size_t end = std::min(e.size(), s + w);
    for (size_t i = s; i < end; i++) {
        double j = i + lag;
        if (j >= end - 1) break;
        sum += e[i] * envAt(e, j);
        cnt++;
    }
    return cnt ? sum / cnt : 0;
}

double tempoScore(std::vector<float> const& e, size_t s, size_t w, double fps, double bpm) {
    double lag = 60.0 * fps / bpm;
    return acf(e, s, w, lag) + 0.5 * acf(e, s, w, lag * 2) + 0.33 * acf(e, s, w, lag * 4)
         + 0.25 * acf(e, s, w, lag * 0.5);
}

struct LineFit { double a = 0, b = 0, maxResid = 0; };

LineFit fitLine(std::vector<std::pair<double, double>> const& pts, size_t from, size_t to) {
    LineFit f;
    double n = double(to - from);
    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    for (size_t i = from; i < to; i++) {
        sx += pts[i].first; sy += pts[i].second;
        sxx += pts[i].first * pts[i].first; sxy += pts[i].first * pts[i].second;
    }
    double d = n * sxx - sx * sx;
    if (std::abs(d) < 1e-9) { f.a = sy / n; f.b = 0; return f; }
    f.b = (n * sxy - sx * sy) / d;
    f.a = (sy - f.b * sx) / n;
    for (size_t i = from; i < to; i++)
        f.maxResid = std::max(f.maxResid, std::abs(pts[i].second - (f.a + f.b * pts[i].first)));
    return f;
}

} // namespace

float OnsetEnvelope::at(double ms) const {
    return envAt(values, ms / 1000.0 * framesPerSecond);
}

OnsetEnvelope computeOnsetEnvelope(std::vector<float> const& mono, int sampleRate,
                                   std::atomic<float>* progress) {
    OnsetEnvelope env;
    int hop = std::max(1, (int)std::lround(sampleRate / TARGET_FPS));
    env.framesPerSecond = double(sampleRate) / hop;
    if (mono.size() < FFT_SIZE) return env;

    std::vector<float> window(FFT_SIZE);
    for (int i = 0; i < FFT_SIZE; i++) window[i] = float(0.5 - 0.5 * std::cos(2 * PI * i / FFT_SIZE));

    // klatka f jest wysrodkowana w probce f*hop
    size_t frames = mono.size() / hop;
    std::vector<float> flux(frames, 0.f);
    std::vector<float> prev(FFT_SIZE / 2, 0.f), cur(FFT_SIZE / 2);
    std::vector<std::complex<float>> buf(FFT_SIZE);

    for (size_t f = 0; f < frames; f++) {
        long long start = (long long)f * hop - FFT_SIZE / 2;
        for (int i = 0; i < FFT_SIZE; i++) {
            long long idx = start + i;
            float s = (idx >= 0 && idx < (long long)mono.size()) ? mono[idx] : 0.f;
            buf[i] = { s * window[i], 0.f };
        }
        fft(buf);
        float sum = 0;
        for (int k = 1; k < FFT_SIZE / 2; k++) {
            cur[k] = std::log1p(100.f * std::abs(buf[k]));
            float d = cur[k] - prev[k];
            if (d > 0) sum += d;
        }
        std::swap(prev, cur);
        flux[f] = sum;
        if (progress && (f & 1023) == 0) *progress = 0.6f * f / frames;
    }

    // odejmij lokalna srednia (~0.4 s) zeby zostaly same "uderzenia"
    int half = (int)(0.2 * env.framesPerSecond);
    std::vector<double> prefix(frames + 1, 0.0);
    for (size_t i = 0; i < frames; i++) prefix[i + 1] = prefix[i] + flux[i];
    env.values.resize(frames);
    for (size_t i = 0; i < frames; i++) {
        size_t a = i > (size_t)half ? i - half : 0;
        size_t b = std::min(frames, i + half + 1);
        double mean = (prefix[b] - prefix[a]) / double(b - a);
        env.values[i] = (float)std::max(0.0, flux[i] - mean);
    }
    // normalizacja do 99 percentyla
    std::vector<float> sorted = env.values;
    size_t p = std::min(sorted.size() - 1, (size_t)(sorted.size() * 0.99));
    std::nth_element(sorted.begin(), sorted.begin() + p, sorted.end());
    float norm = sorted[p] > 1e-6f ? sorted[p] : 1.f;
    for (auto& v : env.values) v = std::min(1.f, v / norm);
    if (progress) *progress = 0.6f;
    return env;
}

AnalysisResult analyzeTiming(OnsetEnvelope const& env, AnalyzerSettings const& st,
                             std::atomic<float>* progress) {
    AnalysisResult res;
    auto const& e = env.values;
    double fps = env.framesPerSecond;
    if (e.size() < fps * 4) return res;
    auto toMs = [&](double frame) { return frame * 1000.0 / fps; };

    // 1) lokalne tempo w oknach 8 s co 2 s
    size_t W = (size_t)(8 * fps), H = (size_t)(2 * fps);
    if (W > e.size()) W = e.size();
    std::vector<size_t> starts;
    for (size_t s = 0; s + W <= e.size(); s += H) starts.push_back(s);
    if (starts.empty()) starts.push_back(0);

    std::vector<double> grid;
    for (double b = st.minBpm; b <= st.maxBpm; b += 0.5) grid.push_back(b);
    std::vector<std::vector<double>> scores(starts.size(), std::vector<double>(grid.size()));
    std::vector<double> total(grid.size(), 0.0);
    for (size_t w = 0; w < starts.size(); w++) {
        for (size_t g = 0; g < grid.size(); g++) {
            scores[w][g] = tempoScore(e, starts[w], W, fps, grid[g]);
            total[g] += scores[w][g];
        }
        if (progress) *progress = 0.6f + 0.2f * w / starts.size();
    }
    auto prior = [&](double bpm) {
        double o = std::log2(bpm / st.preferredBpm);
        return std::exp(-0.5 * o * o);
    };
    size_t bestG = 0;
    for (size_t g = 0; g < grid.size(); g++)
        if (total[g] * prior(grid[g]) > total[bestG] * prior(grid[bestG])) bestG = g;
    double globalBpm = grid[bestG];

    // tempo w kazdym oknie, ograniczone do okolicy globalnego (spojnosc oktawy)
    std::vector<double> local(starts.size());
    for (size_t w = 0; w < starts.size(); w++) {
        size_t bg = bestG;
        for (size_t g = 0; g < grid.size(); g++) {
            if (grid[g] < globalBpm / 1.25 || grid[g] > globalBpm * 1.25) continue;
            if (scores[w][g] > scores[w][bg]) bg = g;
        }
        local[w] = grid[bg];
    }
    // filtr medianowy 5
    std::vector<double> med(local.size());
    for (size_t w = 0; w < local.size(); w++) {
        std::vector<double> v;
        for (int d = -2; d <= 2; d++) {
            long long k = (long long)w + d;
            if (k >= 0 && k < (long long)local.size()) v.push_back(local[k]);
        }
        std::nth_element(v.begin(), v.begin() + v.size() / 2, v.end());
        med[w] = v[v.size() / 2];
    }

    // 2) segmenty o stalym tempie
    struct Seg { size_t w0, w1; double bpm; };
    std::vector<Seg> segs;
    segs.push_back({ 0, 0, med[0] });
    for (size_t w = 1; w < med.size(); w++) {
        auto& s = segs.back();
        if (std::abs(med[w] - s.bpm) / s.bpm > st.changeTolerance &&
            w + 1 < med.size() && std::abs(med[w + 1] - s.bpm) / s.bpm > st.changeTolerance) {
            segs.push_back({ w, w, med[w] });
        } else {
            s.w1 = w;
            // srednia biegnaca
            s.bpm += (med[w] - s.bpm) / double(s.w1 - s.w0 + 1);
        }
    }
    if (!st.detectChanges) {
        segs.clear();
        segs.push_back({ 0, med.size() - 1, globalBpm });
    }
    size_t minWins = (size_t)std::max(1.0, st.minSegmentSeconds / 2.0);
    for (bool merged = true; merged && segs.size() > 1;) {
        merged = false;
        for (size_t i = 0; i < segs.size(); i++) {
            if (segs[i].w1 - segs[i].w0 + 1 >= minWins) continue;
            size_t j = i == 0 ? 1 : (i + 1 == segs.size() ? i - 1 :
                (std::abs(segs[i - 1].bpm - segs[i].bpm) < std::abs(segs[i + 1].bpm - segs[i].bpm) ? i - 1 : i + 1));
            segs[j].w0 = std::min(segs[j].w0, segs[i].w0);
            segs[j].w1 = std::max(segs[j].w1, segs[i].w1);
            segs.erase(segs.begin() + i);
            merged = true;
            break;
        }
    }

    // 3) dla kazdego segmentu: dokladne BPM + faza, potem korekta dryfu (pomiar co 8 beatow)
    struct Beat { double beatIdx; double frame; };
    std::vector<TimingPoint> out;
    int barPhase = 0; // ktory beat w takcie wypada na poczatek kolejnego punktu
    for (size_t si = 0; si < segs.size(); si++) {
        auto const& sg = segs[si];
        size_t fs = si == 0 ? 0 : starts[sg.w0] + W / 2 - H / 2;
        size_t fe = si + 1 == segs.size() ? e.size() : starts[segs[si + 1].w0] + W / 2 - H / 2;
        fe = std::min(fe, e.size());
        if (fe <= fs + 2) continue;

        constexpr int BINS = 64;
        double bestScore = -1, bestBpm = sg.bpm, bestPhase = 0;
        for (double bpm = sg.bpm * 0.98; bpm <= sg.bpm * 1.02; bpm += 0.02) {
            double L = 60.0 * fps / bpm;
            double hist[BINS] = {};
            for (size_t f = fs; f < fe; f++) {
                double ph = std::fmod((double)f, L) / L;
                hist[std::min(BINS - 1, (int)(ph * BINS))] += e[f];
            }
            double mean = 0;
            for (double h : hist) mean += h;
            mean /= BINS;
            for (int b = 0; b < BINS; b++) {
                double v = 0.5 * hist[(b + BINS - 1) % BINS] + hist[b] + 0.5 * hist[(b + 1) % BINS] - 2 * mean;
                if (v > bestScore) {
                    bestScore = v; bestBpm = bpm;
                    double l = hist[(b + BINS - 1) % BINS], c = hist[b], r = hist[(b + 1) % BINS];
                    double den = l - 2 * c + r;
                    double off = std::abs(den) > 1e-9 ? 0.5 * (l - r) / den : 0;
                    bestPhase = (b + 0.5 + off) / BINS * L;
                }
            }
        }
        double L = 60.0 * fps / bestBpm;
        double first = bestPhase + std::ceil((fs - bestPhase) / L) * L;

        // pomiar rzeczywistego polozenia beatow w blokach po 8
        std::vector<std::pair<double, double>> meas; // (indeks beatu, klatka)
        constexpr int CHUNK = 8;
        int nBeats = (int)((fe - first) / L);
        for (int c = 0; c + CHUNK <= nBeats + CHUNK / 2; c += CHUNK) {
            double bestS = -1, bestD = 0, sumS = 0;
            int cnt = 0;
            for (double d = -0.12 * L; d <= 0.12 * L; d += 0.25) {
                double s = 0;
                for (int k = c; k < c + CHUNK && k <= nBeats; k++) s += envAt(e, first + k * L + d);
                sumS += s; cnt++;
                if (s > bestS) { bestS = s; bestD = d; }
            }
            double avg = cnt ? sumS / cnt : 0;
            if (bestS > 0.8 && bestS > avg * 1.4) // pomijaj ciche / niepewne fragmenty
                meas.push_back({ c + CHUNK / 2.0 - 0.5, first + (c + CHUNK / 2.0 - 0.5) * L + bestD });
        }

        // dopasowanie kawalkami liniowego modelu t = a + b*beat
        std::vector<std::pair<double, double>> runs; // (beat startowy, beatLength w klatkach) + time
        std::vector<double> runTimes;
        double tolFrames = 0.012 * fps; // 12 ms
        if (meas.size() < 3 || !st.detectChanges) {
            runs.push_back({ 0, L });
            runTimes.push_back(first);
        } else {
            size_t s = 0;
            while (s < meas.size()) {
                size_t e2 = std::min(meas.size(), s + 3);
                LineFit fit = fitLine(meas, s, e2);
                while (e2 < meas.size()) {
                    LineFit f2 = fitLine(meas, s, e2 + 1);
                    if (f2.maxResid > tolFrames) break;
                    fit = f2; e2++;
                }
                // nie pozwol odplynac za daleko od tempa segmentu
                if (fit.b < L * 0.97 || fit.b > L * 1.03) fit.b = L;
                double startBeat = s == 0 ? 0 : std::floor(meas[s].first - CHUNK / 2.0 + 0.5);
                runs.push_back({ startBeat, fit.b });
                runTimes.push_back(fit.a + fit.b * startBeat);
                if (e2 - s < 3 && e2 >= meas.size()) break;
                s = e2;
            }
        }

        for (size_t r = 0; r < runs.size(); r++) {
            TimingPoint p;
            p.time = toMs(runTimes[r]);
            p.beatLength = toMs(runs[r].second);
            p.meter = st.meter;
            if (!out.empty()) {
                auto const& q = out.back();
                if (p.time <= q.time + q.beatLength * 0.5) continue;
                // ciaglosc taktow: przesun punkt na najblizszy "raz"
                long long n = std::llround((p.time - q.time) / q.beatLength);
                int phase = (int)((barPhase + n) % st.meter);
                int adv = (st.meter - phase) % st.meter;
                p.time += adv * p.beatLength;
                if (si + 1 < segs.size() && r + 1 == runs.size() && p.time >= toMs(fe)) continue;
                // jesli prawie identyczne tempo jak poprzednio i dobrze trafia - pomin
                double predicted = q.time + std::llround((p.time - q.time) / q.beatLength) * q.beatLength;
                if (std::abs(p.beatLength - q.beatLength) < 0.05 && std::abs(predicted - p.time) < 3) continue;
                barPhase = 0;
            }
            out.push_back(p);
        }
        if (progress) *progress = 0.8f + 0.2f * (si + 1) / segs.size();
    }
    if (out.empty()) return res;

    // 4) poczatek beatu + "raz" (downbeat) dla pierwszego punktu
    auto& p0 = out.front();
    double L0 = p0.beatLength;
    double firstStrong = p0.time;
    for (double t = p0.time; t < p0.time + 60000; t += L0) {
        if (env.at(t) > 0.25f || env.at(t + 5) > 0.25f) { firstStrong = t; break; }
    }
    // wybierz faze taktu z najsilniejszymi uderzeniami
    double tEnd = out.size() > 1 ? out[1].time : toMs((double)e.size());
    double bestSum = -1;
    int bestM = 0;
    for (int m = 0; m < st.meter; m++) {
        double sum = 0;
        for (double t = firstStrong + m * L0; t < tEnd; t += L0 * st.meter) sum += env.at(t);
        if (sum > bestSum) { bestSum = sum; bestM = m; }
    }
    p0.time = firstStrong + bestM * L0;
    // pierwszy "raz" nie wczesniej niz pierwsze mocne uderzenie (jak offset w osu)
    while (p0.time - L0 * st.meter >= firstStrong - 1e-6) p0.time -= L0 * st.meter;

    res.firstBeatMs = firstStrong;
    res.map.points = out;
    res.map.sort();
    if (progress) *progress = 1.f;
    return res;
}

double nearestOnset(OnsetEnvelope const& env, double t, double windowMs) {
    double best = t, bestV = -1;
    for (double x = t - windowMs; x <= t + windowMs; x += 1.0) {
        float v = env.at(x);
        if (v > bestV) { bestV = v; best = x; }
    }
    return best;
}
