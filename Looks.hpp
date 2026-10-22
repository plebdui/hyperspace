// What the LOOK pass (LookShader.hpp) does, picked by the music: one picture effect at a time, and
// 2-4 camera moves stacked on top of each other for chaos (more the deeper and louder it gets),
// changing on drops or every so often (V and C pick one by hand). And the things that just
// happen: ripples of light on kicks, the picture shattering like glass on big
// drops (figures glimpsed after), the shapes under the cloak when the mids swell, and the grid's
// square turns.
#pragma once

#include "Audio.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

static const int kNumFx = 60, kNumCams = 25;
static const char* const kFxNames[kNumFx] = {
    "no effect", "datamosh", "glitch", "VHS", "halation", "thermal", "night vision", "x-ray", "pixel art",
    "slit-scan", "echo printing", "step printing", "stutter", "freeze frame", "fisheye", "tilt-shift",
    "motion blur", "double exposure", "colour shift", "haze", "light flash", "duplication", "split screen",
    "screen in screen", "zoetrope", "wigglegram", "underwater", "infinite zoom", "morphing", "ripples",
    "magnifiers", "masking", "silhouette", "spotlight", "hard light", "aspect switch", "dreamcore",
    "weirdcore", "digital overlay", "video game", "dystopian", "collage", "mixed media", "point cloud",
    "projection", "reflection", "cinemagraph", "boomerang", "undercranked", "void", "maximalism",
    "surreal melt", "shadow box", "conveyor", "light painting", "jump cuts", "split diopter",
    "rack focus", "iris", "glyph type"};
static const char* const kCamNames[kNumCams] = {
    "steady", "dolly zoom", "crash zoom", "whip pan", "barrel roll", "Dutch angle", "shaky cam",
    "FPV drone", "bullet time", "slow motion", "speed ramp", "falling", "floating", "pan", "tilt",
    "arc", "overhead", "portal", "zoom in", "parallax", "lazy susan", "square turns", "bolt cam",
    "snorricam", "probe lens"};
static const int kCinemagraph = 46;

struct Looks {
    static const int kStack = 4;
    int fx = 0, forcedFx = -1, forcedCam = -1;
    float fxT = 0, fxLen = 10;
    int cams[kStack] = {0, 0, 0, 0};       // the stacked camera moves (0 = an empty slot)
    float camTs[kStack] = {0, 0, 0, 0}, camLens[kStack] = {0, 0, 0, 0};
    float camWs[kStack] = {0, 0, 0, 0};    // how far in each move has faded (0..1)
    // a move that's been replaced fades out in its own slot while the new one fades in: they morph
    int outCams[kStack] = {0, 0, 0, 0};
    float outTs[kStack] = {0, 0, 0, 0}, outWs[kStack] = {0, 0, 0, 0};
    static constexpr float kFade = 1.6f;  // seconds to fade a move in or out
    float timeScale = 1, speed = 1, camBeat = 0, camBass = 0;  // slow motion / bullet time / speed ramps slow the whole trip
    float shatterT = -1, sinceShatter = 99, shatterSeed = 0, figures = 0, cloak = 0, watchers = 0;
    int breakKind = 0;  // how a drop breaks through: 0 glass shatter, 1 white light, 2 torn curtain
    bool ripplesOn = false, breaksOn = true;  // settings: kick ripples, drop breakthroughs
    int forcedBreak = -1;                    // settings: always this breakthrough (-1 = pick at random)
    bool grabNow = false;    // keep this frame (for the shards, or the cinemagraph's still)
    float ripples[6][3];
    int nr = 0;
    float gridTurn = 0, gridTarget = 0, sinceTurn = 0, sinceGrab = 0;
    uint32_t rng = 777;

    Looks()
    {
        for (auto& r : ripples) r[2] = -100;
    }
    float rnd() { rng = rng * 1664525u + 1013904223u; return (rng >> 8) / 16777216.0f; }
    // Keep camera IDs stable, but exclude abrupt moves and unbounded moves
    // whose angle/position wrapping can jump while stacked or fading.
    static bool cameraEnabled(int c)
    {
        switch (c) {
        case 2: case 3: case 5: case 6: case 10: case 17: case 21: case 22:
        case 4: case 8: case 11: case 13: case 18: case 20:
            return false;
        default: return c >= 0 && c < kNumCams;
        }
    }
    static int nextCamera(int c)
    {
        do { c = (c + 1) % kNumCams; } while (!cameraEnabled(c));
        return c;
    }
    void setCam(int i, int c)
    {
        if (!cameraEnabled(c)) c = 0;
        if (cams[i] == c) return;
        if (cams[i]) { outCams[i] = cams[i]; outTs[i] = camTs[i]; outWs[i] = camWs[i]; }
        cams[i] = c;
        camTs[i] = 0;
        camWs[i] = 0;
    }
    // a camera move that isn't already in the stack
    // effects that show held, replayed or strobed old frames: the trip looks like it stopped
    static bool freezes(int e) { return e == 11 || e == 12 || e == 13 || e == 24 || e == 46 || e == 47 || e == 48; }

    int pickCam()
    {
        for (;;) {
            const int c = 1 + (int)(rnd() * (kNumCams - 1));
            if (!cameraEnabled(c)) continue;
            bool used = false;
            for (int k : cams) used |= k == c;
            if (!used) return c;
        }
    }
    // the stack's names, for the status line
    std::string camNames() const
    {
        std::string n;
        for (int c : cams)
            if (c) n += (n.empty() ? "" : " + ") + std::string(kCamNames[c]);
        return n.empty() ? kCamNames[0] : n;
    }

    // `now` is the clock the look shader gets as uTime
    void update(const Features& f, float stage, float now, float dt)
    {
        grabNow = false;
        fxT += dt;
        for (int i = 0; i < kStack; ++i) {
            camTs[i] += dt;  // (a fading-out move holds still: it just settles back)
            camWs[i] = cams[i] ? std::min(1.0f, camWs[i] + dt / kFade) : 0.0f;
            outWs[i] = std::max(0.0f, outWs[i] - dt / kFade);
            if (outWs[i] == 0) outCams[i] = 0;
        }
        sinceGrab += dt;
        // pick: a drop changes things (not too often), otherwise every 10-20 s; sometimes nothing at all
        if (forcedFx >= 0) {
            if (fx != forcedFx) { fx = forcedFx; fxT = 0; grabNow = true; }
        } else if ((f.drop && fxT > 5) || fxT > fxLen) {
            do fx = rnd() < 0.2f ? 0 : 1 + (int)(rnd() * (kNumFx - 1));
            while (freezes(fx));  // (the ones that hold or replay old frames stop the motion: by hand only)
            fxT = 0;
            fxLen = 10 + 10 * rnd();
            grabNow = true;
        }
        if (fx == kCinemagraph && sinceGrab > 8) grabNow = true;
        if (forcedCam >= 0) {  // by hand: just that one
            if (!cameraEnabled(forcedCam)) forcedCam = 0;
            setCam(0, forcedCam);
            for (int i = 1; i < kStack; ++i) setCam(i, 0);
        } else {
            // how many at once: 2, a 3rd deeper in, a 4th when it's loud
            const int want = 2 + (stage > 1.5f) + (f.level > 0.6f);
            const bool reshuffle = f.drop && camTs[0] > 3;
            for (int i = 0; i < kStack; ++i) {
                if (i >= want) { if (cams[i] && camTs[i] > 2 && !outCams[i]) setCam(i, 0); continue; }  // (let extras finish a bit first)
                // (a slot still fading out its last move waits for it, so no move is cut off mid-fade)
                if (outCams[i]) continue;
                if (!cams[i] || reshuffle || camTs[i] > camLens[i] || (f.beat > 0.999f && rnd() < 0.06f)) {
                    setCam(i, pickCam());
                    camLens[i] = 5 + 9 * rnd();
                }
            }
        }
        float target = 1;  // the slowest move in the stack sets the pace
        for (int c : cams) {
            if (c == 8) target = std::min(target, 0.6f);  // slowed, never stopped: the flight keeps going
            else if (c == 9) target = std::min(target, 0.7f);
            else if (c == 10) target = std::min(target, f.beat > 0.4f ? 1.6f : 0.75f);
        }
        speed += (target - speed) * std::min(1.0f, dt * 2.5f);  // (lurching, but never in one frame)
        camBeat += (f.beat - camBeat) * std::min(1.0f, dt * 11);
        camBass += (std::min(f.bass, 2.5f) * 0.6f - camBass) * std::min(1.0f, dt * 5);
        timeScale = speed;

        // ripples on the kicks
        if (ripplesOn && f.beat > 0.999f) {
            ripples[nr][0] = (rnd() - 0.5f) * 1.4f;
            ripples[nr][1] = (rnd() - 0.5f) * 0.8f;
            ripples[nr][2] = now;
            nr = (nr + 1) % 6;
        }

        // the shatter, once you're past the come-up, on a big drop
        sinceShatter += dt;
        if (breaksOn && f.drop && sinceShatter > 8 && stage > 0.8f) {
            shatterT = 0;
            sinceShatter = 0;
            shatterSeed = rnd() * 100;
            breakKind = forcedBreak >= 0 ? forcedBreak : (int)(rnd() * 3);
            grabNow = true;
        } else if (shatterT >= 0) {
            shatterT += dt;
            if (shatterT > 2.5f) shatterT = -1;
        }
        auto smooth = [](float a, float b, float x) { x = std::clamp((x - a) / (b - a), 0.0f, 1.0f); return x * x * (3 - 2 * x); };
        figures = smooth(0.6f, 1.2f, sinceShatter) * (1 - smooth(3.5f, 4.8f, sinceShatter)) * (breakKind == 0);
        // the watchers come and go once you're through (about half the time)
        const float wantW = stage > 1.6f && std::sin(now * 0.11f) > -0.1f ? 1.0f : 0.0f;
        watchers += (wantW - watchers) * std::min(1.0f, dt * 0.5f);
        if (grabNow) sinceGrab = 0;

        // the cloak comes when the mids swell, deeper in
        const float want = std::clamp(f.mid - 0.9f, 0.0f, 1.0f) * std::min(stage / 1.5f, 1.0f);
        cloak += (want - cloak) * std::min(1.0f, dt * 1.5f);

        // The grid's rapid quarter-turns are disabled with the abrupt cameras.
    }
};
