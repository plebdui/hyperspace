// The ears: what your speakers are playing (or the mic, or a built-in demo
// track), and what the visuals need from it: bass, mids, highs, beats, how
// bright or dark the music sounds, and how far along the trip is.
#pragma once

#include <pulse/error.h>
#include <pulse/simple.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

static constexpr int kRate = 48000;

// ---- the sound coming in, kept in a ring (mono)
class Capture {
public:
    enum Source { kSpeakers, kMic, kDemo };

    ~Capture() { stop(); }

    // false (with why()) if the sound server wouldn't give us anything
    bool start(Source src)
    {
        fSrc = src;
        fRun = true;
        if (src != kDemo) {
            pa_sample_spec ss{PA_SAMPLE_FLOAT32LE, kRate, 2};
            pa_buffer_attr ba;
            ba.maxlength = (uint32_t)-1;
            ba.tlength = ba.prebuf = ba.minreq = (uint32_t)-1;
            ba.fragsize = 512 * 2 * sizeof(float);  // small pieces: low delay between sound and picture
            int err = 0;
            fPa = pa_simple_new(nullptr, "hyperspace", PA_STREAM_RECORD, src == kMic ? nullptr : "@DEFAULT_MONITOR@",
                                "visualizer", &ss, nullptr, &ba, &err);
            if (!fPa) {
                fWhy = pa_strerror(err);
                return false;
            }
        }
        fThread = std::thread([this] { loop(); });
        return true;
    }
    void stop()
    {
        fRun = false;
        if (fThread.joinable()) fThread.join();
        if (fPa) pa_simple_free(fPa);
        fPa = nullptr;
    }
    const std::string& why() const { return fWhy; }

    // the newest n samples
    void latest(float* out, int n)
    {
        std::lock_guard<std::mutex> g(fLock);
        for (int i = 0; i < n; ++i) out[i] = fRing[(fW - n + i) & kMask];
    }

private:
    static constexpr int kSize = 1 << 15, kMask = kSize - 1;

    void push(const float* stereo, int frames)
    {
        std::lock_guard<std::mutex> g(fLock);
        for (int i = 0; i < frames; ++i) fRing[(fW++) & kMask] = 0.5f * (stereo[2 * i] + stereo[2 * i + 1]);
    }

    void loop()
    {
        float buf[512 * 2];
        auto next = std::chrono::steady_clock::now();
        while (fRun) {
            if (fSrc == kDemo) {
                demo(buf, 512);
                next += std::chrono::microseconds(512 * 1000000LL / kRate);
                std::this_thread::sleep_until(next);
            } else if (pa_simple_read(fPa, buf, sizeof(buf), nullptr) < 0) {
                break;
            }
            push(buf, 512);
        }
    }

    // a built-in track for trying it without music: 124 bpm, a 64-bar cycle that
    // starts as a quiet pad, adds hats, then kick and bass, with a drop at bar 16
    void demo(float* out, int frames)
    {
        const double beat = 60.0 / 124;
        for (int i = 0; i < frames; ++i, fT += 1.0 / kRate) {
            const double bars = fT / (4 * beat), inBar = std::fmod(bars, 64.0);
            const double b = std::fmod(fT, beat) / beat;  // where in the beat
            float v = 0;
            // pad: a slow minor chord, always there
            for (double hz : {110.0, 130.81, 164.81, 196.0})
                v += 0.05f * (float)std::sin(2 * M_PI * hz * fT + 0.3 * std::sin(fT * 0.7));
            if (inBar >= 8) {  // hats on the off-beats
                fRng = fRng * 1664525u + 1013904223u;
                const double off = std::fmod(b + 0.5, 1.0);
                v += 0.12f * ((fRng >> 8) / 8388608.0f - 1) * (float)std::exp(-off * 40);
            }
            if (inBar >= 16 && inBar < 56) {  // kick and bass
                const double ph = 2 * M_PI * (50 * b * beat + 60 * beat * (1 - std::exp(-b * 30)) / 30);
                v += 0.8f * (float)(std::sin(ph) * std::exp(-b * 6));
                const double bassHz = (std::fmod(bars, 2.0) < 1) ? 55 : 49;
                v += 0.25f * (float)(std::fmod(bassHz * fT, 1.0) * 2 - 1) * (float)std::min(1.0, b * 8) * (float)std::exp(-b * 2);
            }
            v *= 0.6f;
            out[2 * i] = out[2 * i + 1] = v;
        }
    }

    Source fSrc = kSpeakers;
    pa_simple* fPa = nullptr;
    std::string fWhy;
    std::thread fThread;
    std::atomic<bool> fRun{true};
    std::mutex fLock;
    std::vector<float> fRing = std::vector<float>(kSize, 0.0f);
    uint32_t fW = 0;
    double fT = 0;
    uint32_t fRng = 1;
};

// ---- what the visuals use, worked out every frame
struct Features {
    float bass = 0, mid = 0, high = 0;  // each about 1 at its usual level, more when louder than usual
    float level = 0;                     // how loud overall, 0 (silence) .. 1
    float beat = 0;                      // jumps to 1 on a kick, then fades
    bool drop = false;                   // a big hit this frame
    float mood = 0.5f;                   // 0 dark, 1 bright (where the sound sits, low or high)
};

class Analysis {
public:
    static constexpr int kN = 2048;

    Features update(const float* x, float dt)
    {
        // spectrum (Hann window)
        std::vector<std::complex<float>> a(kN);
        float rms = 0;
        for (int i = 0; i < kN; ++i) {
            const float w = 0.5f - 0.5f * std::cos(2 * (float)M_PI * i / (kN - 1));
            a[i] = x[i] * w;
            rms += x[i] * x[i];
        }
        rms = std::sqrt(rms / kN);
        fft(a);
        auto band = [&](float lo, float hi) {
            const int i0 = std::max(1, (int)(lo * kN / kRate)), i1 = std::min(kN / 2, (int)(hi * kN / kRate));
            float s = 0;
            for (int i = i0; i < i1; ++i) s += std::norm(a[i]);
            return s / std::max(1, i1 - i0);
        };
        const float pb = band(30, 150), pm = band(300, 2000), ph = band(3000, 12000);
        // where the sound sits (spectral centroid), slowly
        float num = 0, den = 1e-12f;
        for (int i = 2; i < kN / 2; ++i) {
            const float m = std::abs(a[i]);
            num += m * i;
            den += m;
        }
        const float centroid = num / den * kRate / kN;

        // each band against its own recent average (so it works at any volume)
        auto rel = [&](float p, float& avg) {
            avg += (p - avg) * (1 - std::exp(-dt / 2.5f));
            return std::clamp(std::sqrt(p / (avg + 1e-12f)), 0.0f, 4.0f);
        };
        const float rb = rel(pb, fAvgB), rm = rel(pm, fAvgM), rh = rel(ph, fAvgH);
        auto follow = [&](float& v, float target) {  // fast up, slower down
            v += (target - v) * (1 - std::exp(-dt / (target > v ? 0.03f : 0.25f)));
        };
        const bool silent = rms < 2e-4f;
        follow(f.bass, silent ? 0 : rb);
        follow(f.mid, silent ? 0 : rm);
        follow(f.high, silent ? 0 : rh);
        const float db = 20 * std::log10(rms + 1e-9f);
        f.level += (std::clamp((db + 48) / 34, 0.0f, 1.0f) - f.level) * (1 - std::exp(-dt / 0.3f));
        if (!silent) {
            const float m = std::clamp((std::log2(std::max(centroid, 1.0f)) - std::log2(700.0f)) / (std::log2(4500.0f) - std::log2(700.0f)), 0.0f, 1.0f);
            f.mood += (m - f.mood) * (1 - std::exp(-dt / 5.0f));
        }

        // beats: the bass jumping well above where it was
        fSinceBeat += dt;
        f.drop = false;
        f.beat *= std::exp(-dt * 6);
        const float flux = rb - fPrevB;
        fPrevB = rb;
        if (!silent && rb > 1.3f && flux > 0.25f && fSinceBeat > 0.2f) {
            f.beat = 1;
            fSinceBeat = 0;
            // a drop: a much bigger hit than the recent ones (after a quieter stretch)
            f.drop = rb > 2.0f && fCalm > 2.0f;
            fCalm = 0;
        }
        fCalm = rb < 1.1f ? fCalm + dt : std::max(0.0f, fCalm - dt * 0.5f);
        return f;
    }

private:
    static void fft(std::vector<std::complex<float>>& a)
    {
        const int n = (int)a.size();
        for (int i = 1, j = 0; i < n; ++i) {
            int bit = n >> 1;
            for (; j & bit; bit >>= 1) j ^= bit;
            j ^= bit;
            if (i < j) std::swap(a[i], a[j]);
        }
        for (int len = 2; len <= n; len <<= 1) {
            const float ang = -2 * (float)M_PI / len;
            const std::complex<float> wl(std::cos(ang), std::sin(ang));
            for (int i = 0; i < n; i += len) {
                std::complex<float> w(1);
                for (int j = 0; j < len / 2; ++j) {
                    const auto u = a[i + j], v = a[i + j + len / 2] * w;
                    a[i + j] = u + v;
                    a[i + j + len / 2] = u - v;
                    w *= wl;
                }
            }
        }
    }

    Features f;
    float fAvgB = 1e-6f, fAvgM = 1e-6f, fAvgH = 1e-6f, fPrevB = 0, fSinceBeat = 0, fCalm = 0;
};

// ---- the trip: how far along (0 come-up, 1 chrysanthemum, 2 breakthrough, 3 hyperspace),
// and which entity is showing
enum Entity { kWatcher = 0, kJesters, kMantis, kNumEntities };
static const char* const kEntityNames[kNumEntities] = {"the Watcher", "the Jesters", "the Mantis"};
static const char* const kStageNames[4] = {"come-up", "chrysanthemum", "breakthrough", "hyperspace"};

class Trip {
public:
    float stage = 0;        // 0..3, smooth
    float entityAmt = 0;    // 0..1 how present the entity is
    int entity = kWatcher;
    int forcedStage = -1;   // a number key holds the trip at a stage (-1: the music decides)
    int forcedEntity = -1;  // E picks the entity yourself (-1: the music's mood decides)

    // back to the start of the come-up (a new picture dropped in)
    void restart()
    {
        fProgress = stage = 0;
        fEntityTime = 0;
    }

    void update(const Features& f, float dt)
    {
        if (forcedStage >= 0) {
            fProgress += (forcedStage - fProgress) * (1 - std::exp(-dt / 0.8f));
        } else {
            // energy carries you deeper; drops push you through; quiet brings you back down
            if (f.level > 0.3f) fProgress += dt * 0.022f * (f.level - 0.3f) / 0.7f * (1 + 2 * f.beat);
            if (f.drop) fProgress += 0.25f;
            if (f.level < 0.15f) fProgress -= dt * 0.1f;
            fProgress = std::clamp(fProgress, 0.0f, 3.0f);
        }
        stage += (fProgress - stage) * (1 - std::exp(-dt / 1.5f));

        // entities: in hyperspace they come and go; a new one on a big drop or every 30 s
        fEntityTime += dt;
        const bool want = stage > 2.5f;
        int pick = forcedEntity >= 0 ? forcedEntity : (f.mood > 0.55f ? kJesters : (fDarkAlt ? kMantis : kWatcher));
        if (want && (f.drop || fEntityTime > 30) && fEntityTime > 12 && forcedEntity < 0) {
            fDarkAlt = !fDarkAlt;
            pick = f.mood > 0.55f ? kJesters : (fDarkAlt ? kMantis : kWatcher);
            fSwap = pick != entity;
        }
        if (forcedEntity >= 0 && forcedEntity != entity) { fSwap = true; pick = forcedEntity; }
        if (fSwap) {  // fade the old one out, then the new one in
            entityAmt -= dt / 1.5f;
            if (entityAmt <= 0) {
                entityAmt = 0;
                entity = pick;
                fSwap = false;
                fEntityTime = 0;
            }
        } else {
            if (!want && entityAmt == 0) entity = pick;  // ready for next time
            entityAmt = std::clamp(entityAmt + (want ? dt / 3 : -dt / 2), 0.0f, 1.0f);
        }
    }

private:
    float fProgress = 0, fEntityTime = 0;
    bool fSwap = false, fDarkAlt = false;
};
