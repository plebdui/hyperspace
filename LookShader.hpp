// LOOK: the film tricks (after the techniques catalogued on eyecannndy.com), done to the finished picture.
//   CAMERA  up to four camera moves stacked at once: dolly zoom, crash zoom, whip pan, barrel roll, Dutch angle, shaky cam,
//           FPV drone, bullet time, ... (Looks.hpp has the full list and picks them)
//   EFFECT  one picture effect at a time: datamosh, glitch, VHS, thermal, slit-scan, infinite zoom, ...
//           Many reach back in time: the last 48 pictures are kept (30 a second, so 1.6 s) in uHist.
// and, whatever's picked, the things from the trip report that just happen with the music:
//   shapes under a cloak that bends the light around them, ripples of light on the kicks, fireworks
//   of sacred geometry rapid-firing on the highs, and on big drops the picture shatters like glass, the outlines of figures glimpsed
//   in the chaos (or a white-light wipe-out the world reassembles from, or a curtain torn open).
// Plus what recurs across many reported DMT trips (Erowid): at the onset, rainbow edges, red/green
// double images, venetian-blind colour seams, the room's tint cycling, static in the corners and
// comic-book outlines; later, heads peering in at the edges of vision and eyes popping open there;
// and fireworks of multiplying squares that fold into cubes, moving in digital steps.
// COPY puts a picture into the history.
#pragma once

static const char* const kCopy = R"(#version 330 core
out vec4 frag;
uniform sampler2D uSrc;
uniform vec2 uRes;
void main() { frag = vec4(texture(uSrc, gl_FragCoord.xy / uRes).rgb, 1.0); }
)";

static const char* const kLook = R"(#version 330 core
out vec4 frag;
uniform sampler2D uScene, uFrozen;
uniform sampler2DArray uHist;
uniform int uHead, uFx;
uniform int uCams[8];       // the camera moves stacked on top of each other (0 = none): 4 going, 4 fading out
uniform float uCamTs[8];    // how long each has been going
uniform float uCamWs[8];
uniform int uCamTest;
uniform float uCamBeat, uCamBass;  // (uCamBass: the bass, smoothed, for the camera)     // the kick, with a quick smooth attack (for the camera: a punch, not a jump)       // (testing: show only the camera, looking at a checkerboard)    // how much of each (they fade in and out, so one move morphs into the next)
uniform vec2 uRes;
uniform float uTime, uBass, uMid, uHigh, uLevel, uBeat, uHue, uMood;
uniform float uFxT, uSpeed, uShatterT, uShatterSeed, uFigures, uCloak;
uniform float uStage, uBreakKind, uWatchers;
uniform float uCalm;  // quiet the trip's own colour overlays (a dimension, or firework-watching)
uniform float uSpread;  // how wide a slice of the spectrum (music-reactive): small = a cohesive scheme, 1 = full rainbow
uniform float uIntro;  // 1 just after a picture is dropped: leave it be for now
uniform vec3 uSeedPal[3];  // a dropped picture's three main colours...
uniform float uSeedMix;     // ...and how much they colour everything  // the trip's stage; how a drop breaks through (glass / white light / torn curtain); the peripheral watchers
uniform vec4 uBursts[16];  // fireworks: x, y, when, which kind
uniform vec3 uRipples[6];  // ripples: x, y, when

#define PI 3.14159265
#define HN 48.0

float asp, px;
vec2 gBlur;   // the camera's motion blur: a smear along this...
float gRad;   // ...and toward the middle
float gRing;  // the portal's rim

mat2 rot(float a) { float c = cos(a), s = sin(a); return mat2(c, -s, s, c); }
float hash(vec2 p) { p = fract(p * vec2(123.34, 456.21)); p += dot(p, p + 45.32); return fract(p.x * p.y); }
float noise(vec2 p)
{
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x), f.y);
}
float fbm(vec2 p)
{
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 4; i++) { s += a * noise(p); p = rot(0.5) * p * 2.03 + 11.0; a *= 0.5; }
    return s;
}
float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }
// a dropped picture's colours, going round
vec3 seedRamp(float t)
{
    t = fract(t) * 3.0;
    vec3 c = t < 1.0 ? mix(uSeedPal[0], uSeedPal[1], t) : (t < 2.0 ? mix(uSeedPal[1], uSeedPal[2], t - 1.0) : mix(uSeedPal[2], uSeedPal[0], t - 2.0));
    return c / max(max(c.r, c.g), max(c.b, 0.08)) * 0.95;  // as bright as it'll go
}
// the same colours as the scene: a rainbow pulled toward pink and green
vec3 pal(float t)
{
    t = uHue + (t - uHue) * uSpread;  // keep colours in a drifting band, not the whole spectrum at once
    vec3 bright = 0.5 + 0.5 * cos(6.2831 * (t + vec3(0.0, 0.33, 0.67)));
    bright = mix(bright, mix(vec3(1.0, 0.2, 0.65), vec3(0.2, 1.0, 0.5), 0.5 + 0.5 * cos(6.2831 * t)), 0.4);
    vec3 dark = vec3(0.32, 0.18, 0.42) + vec3(0.38, 0.32, 0.45) * cos(6.2831 * (t + vec3(0.62, 0.42, 0.18)));
    vec3 c = mix(dark, bright, uMood);
    return uSeedMix > 0.0 ? mix(c, seedRamp(t) * mix(0.6, 1.0, uMood), 0.65 * uSeedMix) : c;
}
vec3 hueShift(vec3 c, float a)
{
    const vec3 k = vec3(0.57735);
    float ca = cos(a * 6.2831), sa = sin(a * 6.2831);
    return max(c * ca + cross(k, c) * sa + k * dot(k, c) * (1.0 - ca), 0.0);
}
float sdSeg(vec2 p, vec2 a, vec2 b)
{
    vec2 pa = p - a, ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba * h);
}
float ease(float x) { x = clamp(x, 0.0, 1.0); return 1.0 - pow(1.0 - x, 4.0); }
float bayer(vec2 f)
{
    ivec2 i = ivec2(mod(f, 4.0));
    int m[16] = int[](0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5);
    return (float(m[i.y * 4 + i.x]) + 0.5) / 16.0;
}

// ---- reading the picture. Points are in picture units: height -0.5..0.5, width by the aspect.
// Off the edge it carries on mirrored, so the camera can go anywhere.
vec2 toUV(vec2 p) { return p / vec2(asp, 1.0) + 0.5; }
vec3 S(vec2 p) { return texture(uScene, toUV(p)).rgb; }
vec3 F(vec2 p) { return texture(uFrozen, toUV(p)).rgb; }
// the picture `age` thirtieths of a second ago
vec3 H(vec2 p, float age)
{
    age = clamp(floor(age), 0.0, HN - 1.0);
    if (age < 0.5) return S(p);
    return texture(uHist, vec3(toUV(p), mod(float(uHead) - age + HN, HN))).rgb;
}
vec3 Hs(vec2 p, float age) { return mix(H(p, age), H(p, age + 1.0), fract(age)); }
// with the camera's motion blur
vec3 B(vec2 p)
{
    if (gRad + abs(gBlur.x) + abs(gBlur.y) < 0.002) return S(p);
    vec3 c = vec3(0.0);
    for (int i = 0; i < 10; i++) {
        float k = float(i) / 9.0;
        c += S(p + gBlur * (k - 0.5) - p * gRad * k);
    }
    return c / 10.0;
}
vec3 blurS(vec2 p, float r)
{
    vec3 c = S(p);
    for (int i = 1; i < 12; i++) {
        float a = float(i) * 2.39996, d = sqrt(float(i) / 11.0) * r;
        c += S(p + d * vec2(cos(a), sin(a)));
    }
    return c / 12.0;
}
float edge(vec2 p)
{
    vec2 e = vec2(1.5 * px, 0.0);
    float gx = luma(S(p + e.xy)) - luma(S(p - e.xy)), gy = luma(S(p + e.yx)) - luma(S(p - e.yx));
    return sqrt(gx * gx + gy * gy) * 2.0;
}

// ---- CAMERA: where this pixel looks, for one move
vec2 move(int cam, float tt, float w, vec2 p)
{
    float t = tt, k = w * w * (3.0 - 2.0 * w);  // how much of it: eased in and out
    vec2 p0 = p;
    vec2 o = vec2(0.0);
    float z = 1.0, a = 0.0;
    vec2 bl = vec2(0.0);
    float rd = 0.0, rg = 0.0;
    if (cam == 1) {         // dolly zoom: the middle pushes in while the edges pull away (Vertigo)
        float s = sin(t * 0.7) * k;
        p *= 1.0 + 1.4 * s * dot(p, p);
        z = 1.0 + 0.4 * s;
    } else if (cam == 2) {  // crash zoom on every kick
        z = 1.0 + 1.1 * uCamBeat;
        rd = 0.2 * uCamBeat;
    } else if (cam == 3) {  // whip pan: swooping sideways to a new view, smeared
        float ph = fract(t / 1.8);
        o.x = (floor(t / 1.8) + smoothstep(0.0, 0.5, ph)) * asp * 1.1;  // (a swoop, not a cut)
        bl = vec2(0.25 * sin(PI * clamp(ph / 0.5, 0.0, 1.0)), 0.0);
    } else if (cam == 4) {  // barrel roll
        a = t * 1.1 + uCamBass * 0.4;
        z = 1.15;
    } else if (cam == 5) {  // Dutch angle, snapping to a new tilt now and then
        float seg = floor(t / 2.4);
        a = mix(hash(vec2(seg - 1.0, 1.0)) - 0.5, hash(vec2(seg, 1.0)) - 0.5, smoothstep(0.0, 0.6, fract(t / 2.4))) * 0.9;
        z = 1.2;
    } else if (cam == 6) {  // shaky cam, worse on the bass
        o = (vec2(noise(vec2(t * 7.0, 0.0)), noise(vec2(0.0, t * 7.0 + 3.0))) - 0.5) * (0.05 + 0.08 * uCamBass);
        a = (noise(vec2(t * 5.0, 9.0)) - 0.5) * 0.08;
        z = 1.08;
    } else if (cam == 7) {  // FPV drone: banking, diving, swooping
        a = 0.45 * sin(t * 0.8) + 0.15 * sin(t * 2.1);
        z = 1.2 + 0.2 * sin(t * 1.3);
        o = vec2(0.15 * sin(t * 0.6), 0.08 * sin(t * 1.1));
        p /= 1.0 + 0.25 * sin(t * 0.5) * p.y;
        rd = 0.05 + 0.08 * uCamBass;
    } else if (cam == 8) {  // bullet time: nearly frozen, the camera circling
        a = t * 0.4;
        z = 1.25 + 0.1 * sin(t * 0.3);
        p /= 1.0 + 0.35 * sin(t * 0.4) * p.x;
    } else if (cam == 9) {  // slow motion
        z = 1.05 + 0.1 * sin(t * 0.2);
    } else if (cam == 10) { // speed ramp: lurching between fast and slow
        z = 1.0 + 0.15 * uSpeed;
        rd = 0.07 * max(uSpeed - 1.0, 0.0);
    } else if (cam == 11) { // falling
        o.y = t * 0.7;
        a = 0.25 * sin(t * 0.4);
        p /= 1.0 - 0.35 * p.y;
    } else if (cam == 12) { // floating
        o = 0.06 * vec2(sin(t * 0.31), sin(t * 0.23));
        a = 0.1 * sin(t * 0.19);
        z = 1.1 + 0.05 * sin(t * 0.27);
    } else if (cam == 13) { // pan
        o.x = t * 0.3;
    } else if (cam == 14) { // tilt up and down
        o.y = 0.4 * sin(t * 0.45);
        p /= 1.0 + 0.3 * sin(t * 0.45) * p.y;
    } else if (cam == 15) { // arc: swinging round the middle
        p /= 1.0 + 0.5 * sin(t * 0.6) * p.x;
        o.x = 0.25 * sin(t * 0.6);
        a = 0.1 * sin(t * 0.6);
    } else if (cam == 16) { // overhead to worm's-eye
        p /= 1.0 + 0.9 * sin(t * 0.35) * p.y;
        z = 1.2;
    } else if (cam == 17) { // portal: a ring opens and you go through it
        float ph = fract(t / 3.0), R = ph * ph * 1.8, r = length(p);
        if (r < R) p *= 0.25 + 0.75 * ph;
        else p /= 1.0 + 3.0 * ph * ph;
        rg = exp(-pow((r - R) * 40.0, 2.0)) * (1.0 - ph);
        rd = 0.12 * ph;
    } else if (cam == 18) { // zoom in, turning
        z = 1.3 + 0.3 * sin(t * 0.35);
        a = 0.1 * t;
        rd = 0.1 + 0.1 * uCamBass;
    } else if (cam == 19) { // parallax: bright things are near, so they slide more
        p.x += sin(t * 0.8) * 0.07 * luma(S(p));
    } else if (cam == 20) { // lazy susan: the world on a turntable
        p.y *= 1.6;
        a = t * 0.6;
    } else if (cam == 21) { // square turns: the whole view snaps round 90 degrees
        a = (floor(t / 3.0) + smoothstep(0.0, 0.55, fract(t / 3.0))) * PI * 0.5;
        z = 1.25;
    } else if (cam == 22) { // bolt cam: robot-arm lunges, dead stops
        float seg = floor(t / 1.3), e = smoothstep(0.0, 0.45, fract(t / 1.3));
        vec3 A = vec3(hash(vec2(seg - 1.0, 2.0)) - 0.5, (hash(vec2(seg - 1.0, 3.0)) - 0.5) * 0.8, 1.0 + 0.8 * hash(vec2(seg - 1.0, 4.0)));
        vec3 C = vec3(hash(vec2(seg, 2.0)) - 0.5, (hash(vec2(seg, 3.0)) - 0.5) * 0.8, 1.0 + 0.8 * hash(vec2(seg, 4.0)));
        vec3 m = mix(A, C, e) * vec3(0.5, 0.5, 1.0);
        o = m.xy;
        z = m.z;
        bl = (C.xy - A.xy) * 0.5 * (1.0 - e);
    } else if (cam == 23) { // snorricam: the middle stays put, the world swings round it
        p = rot(1.6 * sin(t * 0.7) * length(p) * 2.0) * p;
    } else if (cam == 24) { // probe lens: right up close, bulging
        p *= 0.55 + 1.2 * dot(p, p);
        z = 1.0 + 0.15 * sin(t * 0.5);
        rd = 0.05;
    }
    // the bends this move made to the picture, only as much as it's faded in
    p = mix(p0, p, k);
    // spins and drifts the short way round: the picture repeats (mirrored) every 2 across and 2*asp
    // along, and a turn every 2 pi, so a move fading out never has far to unwind
    a = mod(a + PI, 2.0 * PI) - PI;
    o = vec2(mod(o.x + asp, 2.0 * asp) - asp, mod(o.y + 1.0, 2.0) - 1.0);
    gBlur += bl * k;
    gRad += rd * k;
    gRing = max(gRing, rg * k);
    return rot(a * k) * p / mix(1.0, z, k) + o * k;
}
// the stack: each move works on where the one before it looked, so they all pile up
vec2 camera(vec2 p)
{
    gBlur = vec2(0.0);
    gRad = 0.0;
    gRing = 0.0;
    for (int i = 0; i < 8; i++)
        if (uCams[i] > 0 && uCamWs[i] > 0.001) p = move(uCams[i], uCamTs[i], uCamWs[i], p);
    gRad = min(gRad, 0.35);
    return p;
}

// ---- EFFECTS
vec3 fxGlitch(vec2 p)
{
    float band = floor(p.y * 24.0), g = hash(vec2(band, floor(uTime * 12.0)));
    float on = step(0.7 - 0.3 * uHigh, g);
    vec2 q = p + vec2(on * (g - 0.85) * 0.3, 0.0), sp = vec2(0.004 + 0.012 * on, 0.0);
    vec3 c = vec3(B(q + sp).r, B(q).g, B(q - sp).b);
    if (hash(floor(gl_FragCoord.xy / vec2(64.0, 16.0)) + floor(uTime * 8.0)) > 0.96) c = 1.0 - clamp(c.gbr, 0.0, 1.0);
    return c * (0.85 + 0.15 * step(0.5, fract(gl_FragCoord.y * 0.5)));
}
vec3 fxEcho(vec2 p)
{
    vec3 c = B(p);
    for (int k = 1; k <= 4; k++) c = max(c, hueShift(H(p, float(k) * 4.0), float(k) * 0.09) * (1.0 - float(k) * 0.18));
    return c;
}
vec3 fxDuplicate(vec2 p)
{
    float n = 2.0 + mod(floor(uFxT / 6.0), 2.0);
    vec2 g = toUV(p) * n, id = floor(g), f = fract(g);
    float k = id.x + id.y * n;
    vec3 c = hueShift(H((f - 0.5) * vec2(asp, 1.0), k * 3.0), k * 0.12);
    return c * smoothstep(0.0, 0.02, min(min(f.x, 1.0 - f.x), min(f.y, 1.0 - f.y)));
}
vec3 fxHalftone(vec2 p)
{
    float cs = 7.0 * px;
    vec2 rp = rot(0.785) * p, ci = floor(rp / cs), cf = fract(rp / cs) - 0.5;
    vec3 s = S(rot(-0.785) * (ci + 0.5) * cs);
    float dots = smoothstep(0.05, 0.0, length(cf) - 0.5 * sqrt(luma(s)));
    vec3 paper = vec3(0.95, 0.9, 0.8) * (0.9 + 0.1 * noise(p * 80.0));
    vec3 c = mix(paper, s * 1.1, dots);
    return c * (1.0 - 0.4 * step(0.5, fract((p.x - p.y) * 90.0)) * smoothstep(0.3, 0.1, luma(s)));  // pencil hatching in the dark
}
vec3 thermal(float t)
{
    t = clamp(t, 0.0, 1.0);
    vec3 c = mix(vec3(0.0, 0.0, 0.1), vec3(0.3, 0.0, 0.6), smoothstep(0.0, 0.25, t));
    c = mix(c, vec3(0.9, 0.1, 0.3), smoothstep(0.25, 0.5, t));
    c = mix(c, vec3(1.0, 0.65, 0.0), smoothstep(0.5, 0.75, t));
    return mix(c, vec3(1.0, 1.0, 0.85), smoothstep(0.75, 1.0, t));
}
float glyph(vec2 f, float h)
{
    const vec2 G[9] = vec2[](vec2(-0.3, -0.3), vec2(0.0, -0.3), vec2(0.3, -0.3), vec2(-0.3, 0.0), vec2(0.0, 0.0),
                             vec2(0.3, 0.0), vec2(-0.3, 0.3), vec2(0.0, 0.3), vec2(0.3, 0.3));
    const ivec2 E[12] = ivec2[](ivec2(0, 1), ivec2(1, 2), ivec2(3, 4), ivec2(4, 5), ivec2(6, 7), ivec2(7, 8),
                               ivec2(0, 3), ivec2(3, 6), ivec2(2, 5), ivec2(5, 8), ivec2(0, 4), ivec2(4, 8));
    float bits = floor(h * 4096.0), d = 1.0;
    for (int i = 0; i < 12; i++)
        if (mod(floor(bits / exp2(float(i))), 2.0) > 0.5) d = min(d, sdSeg(f, G[E[i].x], G[E[i].y]));
    return smoothstep(0.09, 0.07, d);
}

vec3 effect(vec2 p)
{
    vec2 fc = gl_FragCoord.xy;
    float t = uFxT;
    vec3 c = vec3(0.0);
    switch (uFx) {
    case 1: {  // datamosh: blocks of an old picture dragged along by the new one's movement
        vec2 cell = floor(fc / 20.0);
        float h = hash(cell + floor(t * 1.5) * 7.1);
        vec2 mv = (vec2(noise(cell * 0.12 + uTime * 0.4), noise(cell * 0.12 - uTime * 0.4 + 9.0)) - 0.5) * (0.08 + 0.15 * uBass);
        c = B(p);
        if (h < 0.35 + 0.35 * uBass) c = mix(H(p - mv, 2.0 + floor(hash(cell + 5.0) * 14.0)), c * vec3(1.1, 0.9, 1.2), 0.25);
        break;
    }
    case 2: c = fxGlitch(p); break;
    case 3: {  // VHS: wobbling lines, smeared colour, a tracking band rolling through
        float roll = fract(uTime * 0.12), track = smoothstep(0.04, 0.0, abs(fract(p.y + 0.5) - roll));
        vec2 q = p + vec2(0.003 * sin(p.y * 40.0 + uTime * 3.0) + 0.002 * sin(p.y * 170.0 - uTime * 11.0)
                          + track * 0.03 * (hash(vec2(floor(fc.y), uTime)) - 0.5), 0.0);
        vec3 a = B(q), b = (S(q - vec2(0.01, 0.0)) + S(q - vec2(0.02, 0.0)) + S(q - vec2(0.03, 0.0))) / 3.0;
        c = vec3(luma(a)) + (b - vec3(luma(b))) * 1.3;
        c = mix(c, vec3(luma(c)), 0.15) * vec3(1.05, 0.97, 1.0);
        c += track * 0.4 * hash(fc + uTime) + (hash(fc * 0.5 + fract(uTime) * 91.0) - 0.5) * 0.08;
        c *= 0.9 + 0.1 * sin(fc.y * PI);
        break;
    }
    case 4: {  // halation: red-orange glow bleeding round the highlights, like film
        c = B(p);
        vec3 h = vec3(0.0);
        for (int i = 0; i < 12; i++) {
            float a = float(i) * 0.5236;
            h += max(S(p + vec2(cos(a), sin(a)) * (0.012 + 0.012 * float(i % 3))) - 0.55, 0.0);
        }
        c = (c + h / 12.0 * vec3(1.0, 0.3, 0.12) * 3.0) * vec3(1.04, 0.98, 0.92);
        break;
    }
    case 5: c = thermal(luma(blurS(p, 0.006)) * 1.25 + 0.15 * uBass) + (hash(fc + uTime) - 0.5) * 0.04; break;
    case 6: {  // night vision, through binoculars
        float l = luma(B(p)) * 2.2 + (hash(fc + fract(uTime) * 77.0) - 0.5) * 0.25;
        c = vec3(0.12, 1.0, 0.35) * l * (0.85 + 0.15 * sin(fc.y * 2.0));
        c *= smoothstep(0.45, 0.4, min(length(p - vec2(-0.27, 0.0)), length(p - vec2(0.27, 0.0))));
        break;
    }
    case 7: c = vec3(0.6, 0.8, 1.0) * (pow(clamp(luma(B(p)) * 1.6, 0.0, 1.0), 1.5) * 0.25 + pow(1.0 - clamp(luma(blurS(p, 0.02)), 0.0, 1.0), 3.0) * 0.35 + edge(p) * 1.8); break;  // x-ray
    case 8: {  // pixel art: big pixels, few colours, dithered
        float bs = (6.0 + floor(uBass * 6.0)) * px;
        vec2 cell = floor(p / bs);
        c = floor(clamp(S((cell + 0.5) * bs), 0.0, 1.0) * 4.0 + bayer(cell)) / 4.0;
        break;
    }
    case 9:  // slit-scan: each ring (or row) of the picture from a different moment
        c = Hs(p, mod(floor(t / 8.0), 2.0) < 0.5 ? length(p) * 95.0 : (p.y + 0.5) * 46.0);
        break;
    case 10: c = fxEcho(p); break;  // echo printing: ghosts of the last moments
    case 11: {  // step printing: each moment printed twice over, overlapping
        float a = mod(floor(uTime * 10.0), 3.0) * 2.0;
        c = mix(H(p, a), H(p, a + 3.0), 0.45);
        break;
    }
    case 12: {  // stutter: skipping back
        float g = hash(vec2(floor(uTime * 6.0), 3.3));
        c = H(p, g < 0.35 + 0.4 * uBeat ? floor(fract(uTime * 6.0) * 4.0) * 3.0 + floor(g * 12.0) : 0.0);
        break;
    }
    case 13: {  // freeze frame: a snapshot flashes up, framed like a photo
        float ph = mod(t, 2.4), m = max(abs(p.x) / (asp * 0.5), abs(p.y) / 0.5);
        c = B(p);
        if (ph < 1.4) {
            vec3 f = H(p * (0.92 - 0.03 * ph), ph * 30.0);
            f = mix(vec3(luma(f)), f, 0.6) * vec3(1.05, 1.0, 0.92);
            c = mix(c * 0.35, f, step(m, 0.9));
            c = mix(c, vec3(1.0), step(0.9, m) * step(m, 0.94));
            c += exp(-ph * 14.0) * 1.5;
        }
        break;
    }
    case 14: {  // fisheye
        float k = 1.2 + 0.8 * uBass;
        c = S(p * (1.0 + k * dot(p, p)) / (1.0 + k * 0.2)) * smoothstep(1.0, 0.92, length(toUV(p) * 2.0 - 1.0));
        break;
    }
    case 15: {  // tilt-shift: only a band in focus, so it all looks like a tiny model
        float d = abs(p.y - 0.12 * sin(uTime * 0.3));
        c = clamp(blurS(p, smoothstep(0.06, 0.4, d) * 0.025), 0.0, 1.0);
        c = clamp(mix(vec3(luma(c)), c, 1.5), 0.0, 1.0);
        c = c * c * (3.0 - 2.0 * c);
        break;
    }
    case 16: {  // motion blur: the last few moments smeared together
        c = B(p);
        for (int k = 1; k < 8; k++) c += H(p * (1.0 - 0.008 * float(k)), float(k));
        c /= 8.0;
        break;
    }
    case 17: {  // double exposure: an older moment, turned and bigger, printed into the bright parts
        vec3 a = B(p), b = H(rot(0.4 * sin(uTime * 0.1)) * p * 1.5 + vec2(0.15, 0.0), 40.0);
        c = mix(b * 0.7, 1.0 - (1.0 - clamp(a, 0.0, 1.0)) * (1.0 - clamp(b, 0.0, 1.0)), smoothstep(0.15, 0.6, luma(a)));
        break;
    }
    case 18: {  // colour shift: the colours jump round, and swap
        float s = floor(t * 1.5);
        c = hueShift(B(p), s * 0.21 + p.x * 0.15);
        float sw = mod(s, 3.0);
        if (sw > 1.5) c = c.brg;
        else if (sw > 0.5) c = c.gbr;
        break;
    }
    case 19: {  // haze
        c = mix(B(p), blurS(p, 0.03), 0.6);
        c = mix(c * 0.85 + 0.06, pal(uHue + 0.1) * 0.5 + 0.45, 0.25 + 0.3 * (p.y + 0.5) * noise(p * 3.0 + uTime * 0.1));
        break;
    }
    case 20: {  // light flashes, flash cuts to negative, a light leak drifting over
        c = B(p);
        if (hash(vec2(floor(uTime * 4.0), 1.7)) > 0.7 && uBeat > 0.6) c = 1.0 - clamp(c, 0.0, 1.0);
        c += uBeat * uBeat * vec3(1.0, 0.95, 0.9);
        c += vec3(1.0, 0.45, 0.25) * 0.7 * smoothstep(0.9, 0.0, length(p - vec2(0.9 * sin(uTime * 0.23) * asp * 0.5, 0.55)));
        break;
    }
    case 21: c = fxDuplicate(p); break;
    case 22: {  // split screen: three panels, each closer in and a little behind the last
        float u = toUV(p).x * 3.0, id = floor(u);
        c = H((p - vec2((id - 1.0) * asp / 3.0, 0.0)) / (1.0 + id * 0.35), id * 10.0);
        c = mix(vec3(1.0), c, smoothstep(0.497, 0.49, abs(fract(u) - 0.5)));
        break;
    }
    case 23: {  // screen in screen: the picture inside itself, forever, zooming in
        float s = 0.6, Z = floor(uTime * 0.2), lvl = 0.0;
        vec2 q = p * pow(s, fract(uTime * 0.2));
        for (int i = 0; i < 6; i++) {
            if (max(abs(q.x) / asp, abs(q.y)) < 0.5 * s) { q /= s; lvl += 1.0; }
        }
        c = hueShift(H(q, lvl * 4.0), (lvl + Z) * 0.13);
        c += vec3(0.9, 1.0, 1.0) * smoothstep(0.006, 0.0, abs(max(abs(q.x) / asp, abs(q.y)) - 0.5 * s)) * 1.2;
        break;
    }
    case 24: {  // zoetrope: slices of different moments going round, strobing
        float a = atan(p.y, p.x) + uTime * 0.5, seg = 6.2831 / 12.0, f = fract(a / seg);
        c = H(p, mod(floor(a / seg) * 4.0, 48.0)) * smoothstep(0.0, 0.08, min(f, 1.0 - f));
        c *= 0.55 + 0.45 * step(0.4, fract(uTime * 12.0));
        break;
    }
    case 25: {  // wigglegram: flicking between two eyes' views
        float side = step(0.5, fract(uTime * 5.0)) * 2.0 - 1.0;
        c = S(rot(side * 0.01) * p + vec2(side * (0.006 + 0.025 * luma(blurS(p, 0.01))), 0.0));
        break;
    }
    case 26: {  // underwater: wobbling, caustics, light shafts
        vec2 q = p + 0.01 * vec2(sin(p.y * 20.0 + uTime * 2.0), cos(p.x * 18.0 + uTime * 1.7));
        c = B(q) * vec3(0.5, 0.9, 1.0);
        float ca = 0.5 + 0.5 * sin(p.x * 30.0 + noise(p * 5.0 + uTime) * 6.0) * sin(p.y * 30.0 + noise(p * 5.0 - uTime) * 6.0);
        c += vec3(0.4, 0.8, 0.9) * pow(ca, 8.0) * 0.5;
        c += vec3(0.5, 0.9, 1.0) * 0.2 * smoothstep(0.65, 1.0, noise(vec2(p.x * 6.0 + p.y * 2.0, uTime * 0.3))) * (p.y + 0.5);
        break;
    }
    case 27: {  // infinite zoom: the picture spiralling into itself (a Droste)
        float r = length(p) + 1e-5, sc = log(2.5), lr = log(r) - uTime * 0.35, a = atan(p.y, p.x) + lr * 0.25;
        c = B(vec2(cos(a), sin(a)) * exp(mod(lr, sc)) * 0.17);
        break;
    }
    case 28: {  // morphing: everything liquid
        vec2 w = vec2(fbm(p * 3.0 + uTime * 0.2), fbm(p * 3.0 - uTime * 0.2 + 5.0)) - 0.5;
        c = S(p + w * (0.15 + 0.2 * uBass));
        break;
    }
    case 29: {  // ripples out from the middle, with the bass
        float r = length(p);
        vec2 q = p + p / (r + 1e-4) * sin(r * 45.0 - uTime * 7.0) * 0.012 * (0.6 + 1.5 * uBass);
        c = vec3(S(q * 1.004).r, S(q).g, S(q * 0.996).b);
        break;
    }
    case 30: {  // magnifying glasses drifting over it
        c = B(p);
        for (int i = 0; i < 3; i++) {
            float fi = float(i);
            vec2 ce = vec2(asp * 0.32 * sin(uTime * 0.4 + fi * 2.1), 0.3 * cos(uTime * 0.33 + fi * 1.3));
            float R = 0.13 + 0.04 * uBass, d = length(p - ce);
            if (d < R) { float k = d / R; c = S(ce + (p - ce) * (0.35 + 0.4 * k * k)) * 1.1; }
            c += vec3(0.85, 0.95, 1.0) * exp(-pow((d - R) * 120.0, 2.0)) * 1.2;
        }
        break;
    }
    case 31: {  // masking: rings / stripes / spokes showing a negative from a moment ago
        float kind = mod(floor(t / 3.0), 3.0), m;
        if (kind < 0.5) m = step(0.5, fract(length(p) * 5.0 - uTime * 0.7));
        else if (kind < 1.5) m = step(0.5, fract((p.x + p.y) * 4.0 + uTime * 0.5));
        else m = step(0.5, fract(atan(p.y, p.x) / 6.2831 * 8.0 + uTime * 0.2));
        c = mix(B(p), hueShift(1.0 - clamp(H(p, 12.0), 0.0, 1.0), 0.5), m);
        break;
    }
    case 32: {  // silhouette: black shapes against a pink and gold sky
        vec3 sky = mix(mix(vec3(1.0, 0.35, 0.55), vec3(1.0, 0.85, 0.4), p.y + 0.5), pal(uHue + 0.2) * 1.2, 0.3);
        c = mix(sky, vec3(0.01), smoothstep(0.32, 0.4, luma(blurS(p, 0.004)))) + sky * edge(p) * 0.5;
        break;
    }
    case 33: {  // spotlights in the dark
        float m = 0.0;
        for (int i = 0; i < 2; i++) {
            float fi = float(i);
            m += smoothstep(0.28, 0.05, length(p - vec2(asp * 0.3 * sin(uTime * 0.5 + fi * 3.0 + uBass), 0.25 * sin(uTime * 0.37 + fi))));
        }
        c = B(p) * (0.04 + 1.5 * m);
        break;
    }
    case 34: {  // hard light: flat colours, black shadows
        c = B(p);
        float l = luma(c);
        c = floor(clamp(c, 0.0, 1.0) * 3.0 + 0.5) / 3.0 * (l < 0.28 ? 0.08 : 1.25);
        break;
    }
    case 35: {  // the frame's shape snapping: cinemascope, square, 4:3, phone
        const float R[5] = float[](2.39, 1.0, 1.333, 0.5625, 1.85);
        float seg = floor(t / 2.0), r = R[int(mod(seg + floor(hash(vec2(seg, 4.0)) * 3.0), 5.0))];
        c = B(p) * (r < asp ? step(abs(p.x), r * 0.5) : step(abs(p.y), asp / r * 0.5));
        break;
    }
    case 36: {  // dreamcore: soft, pale, overexposed
        c = mix(B(p), blurS(p, 0.02), 0.55);
        c = mix(pow(clamp(c, 0.0, 1.5), vec3(0.75)) * 0.8 + vec3(0.18, 0.14, 0.2), vec3(1.0, 0.85, 0.95), 0.15 + 0.1 * uBeat);
        break;
    }
    case 37: {  // weirdcore: a cheap, over-squashed, over-saturated picture
        float bs = 5.0 * px;
        c = S((floor(p / bs) + 0.5) * bs);
        c = floor(clamp(mix(vec3(luma(c)), c, 1.7), 0.0, 1.0) * 5.0) / 5.0;
        vec2 blk = fract(fc / 40.0);
        c += 0.05 * sin(blk.x * 25.0) * sin(blk.y * 25.0);
        break;
    }
    case 38: {  // digital overlay: a targeting display locking on to the bright things
        c = B(p) * 0.85;
        float r = length(p), a = atan(p.y, p.x) / 6.2831;
        float g = smoothstep(0.003, 0.0, abs(r - 0.12)) + smoothstep(0.002, 0.0, abs(r - 0.22)) * step(0.5, fract(a * 24.0 + uTime * 0.3));
        g += (smoothstep(0.002, 0.0, abs(p.x)) + smoothstep(0.002, 0.0, abs(p.y))) * step(r, 0.06);
        g += smoothstep(0.004, 0.0, abs(r - 0.32 - 0.03 * uBass)) * step(0.7, fract(a * 3.0 - uTime * 0.5));
        vec2 gp = p * 5.0, gi = floor(gp), gf = abs(fract(gp) - 0.5);
        float box = smoothstep(0.015, 0.0, abs(max(gf.x, gf.y) - 0.4)) * step(0.25, min(gf.x, gf.y));
        g += box * step(0.62, luma(S((gi + 0.5) / 5.0)));
        g += smoothstep(0.004, 0.0, abs(p.y - (fract(uTime * 0.25) - 0.5))) * 0.6;
        c += vec3(0.3, 1.0, 0.7) * g;
        break;
    }
    case 39: {  // video game: chunky pixels, scanlines, and meters for the bass / mids / highs
        float bs = 4.0 * px;
        c = floor(clamp(S((floor(p / bs) + 0.5) * bs), 0.0, 1.0) * 3.0 + 0.5) / 3.0;
        c *= 0.8 + 0.2 * step(0.5, fract(fc.y / 3.0));
        for (int i = 0; i < 3; i++) {
            float v = i == 0 ? uBass : (i == 1 ? uMid : uHigh);
            vec2 b = p - vec2(-asp * 0.5 + 0.04, 0.44 - float(i) * 0.04);
            if (b.y > 0.0 && b.y < 0.025 && b.x > 0.0 && b.x < 0.3)
                c = b.x < 0.2 * clamp(v, 0.0, 1.5) ? vec3(i == 0 ? 1.0 : 0.2, i == 1 ? 1.0 : 0.3, i == 2 ? 1.0 : 0.3) : vec3(0.1);
        }
        break;
    }
    case 40: {  // dystopian: drained, green-grey, flickering, gritty
        c = B(p);
        c = smoothstep(0.08, 0.9, mix(vec3(luma(c)), c, 0.2) * vec3(0.72, 0.95, 0.82));
        c = c * (0.88 + 0.12 * hash(vec2(floor(uTime * 24.0), 2.0))) + (hash(fc + fract(uTime) * 33.0) - 0.5) * 0.12;
        break;
    }
    case 41: {  // collage: torn-out pieces of other moments stuck on top
        c = B(p) * 0.3;
        float seg = floor(t / 1.5);
        for (int i = 0; i < 7; i++) {
            float fi = float(i);
            vec2 ce = (vec2(hash(vec2(fi, seg)), hash(vec2(fi, seg + 9.0))) - 0.5) * vec2(asp * 0.8, 0.8);
            vec2 sz = 0.12 + 0.18 * vec2(hash(vec2(fi, seg + 3.0)), hash(vec2(fi, seg + 5.0)));
            vec2 q = rot((hash(vec2(fi, seg + 7.0)) - 0.5) * 0.5) * (p - ce), d = abs(q) - sz;
            if (max(d.x, d.y) < 0.0) {
                vec2 src = q + (vec2(hash(vec2(fi, seg + 11.0)), hash(vec2(fi, seg + 13.0))) - 0.5) * 0.4;
                c = max(d.x, d.y) > -0.008 ? vec3(0.95, 0.93, 0.88) : hueShift(H(src, fi * 6.0), fi * 0.1);
            }
        }
        break;
    }
    case 42: c = fxHalftone(p); break;  // mixed media: halftone dots and pencil on paper
    case 43: {  // point cloud, like a 3D scan, swaying
        float cs = 5.0 * px;
        vec2 q = p + vec2(sin(uTime * 0.6) * 0.05 * (luma(S(p)) - 0.4), 0.0), ci = floor(q / cs);
        vec3 s = S((ci + 0.5) * cs);
        c = s * 1.4 * smoothstep(0.35, 0.1, length(fract(q / cs) - 0.5)) * step(0.3, hash(ci)) * step(0.08, luma(s));
        break;
    }
    case 44: {  // projected onto a ribbed wall, dust in the beam
        c = B(p + vec2(0.012 * sin(p.y * 40.0), 0.0)) * (0.55 + 0.45 * sin(p.x * 70.0 + 0.6 * sin(p.y * 6.0))) * 1.3;
        c *= smoothstep(0.9, 0.2, length(p / vec2(asp * 0.6, 0.6)));
        c += vec3(0.9, 0.85, 0.7) * 0.06 * noise(p * 30.0 + uTime) * smoothstep(0.5, 0.0, abs(p.x));
        break;
    }
    case 45: {  // reflected in water
        float hz = -0.08;
        c = p.y > hz ? B(p) : S(vec2(p.x + 0.006 * sin(p.y * 90.0 + uTime * 3.0), 2.0 * hz - p.y)) * vec3(0.55, 0.7, 0.95) * (0.9 - (hz - p.y));
        c += vec3(0.9, 0.95, 1.0) * exp(-abs(p.y - hz) * 300.0) * 0.5;
        break;
    }
    case 46: {  // cinemagraph: a still picture, alive in just one place
        vec2 ce = vec2(asp * 0.25 * sin(uTime * 0.21), 0.15 * cos(uTime * 0.17));
        c = mix(F(p) * 0.95, B(p), smoothstep(0.26, 0.16, length(p - ce)));
        break;
    }
    case 47: {  // boomerang: forwards, backwards, forwards...
        float u = mod(uTime * 30.0, 24.0);
        c = H(p, u + 24.0 - (12.0 - abs(u - 12.0)));
        break;
    }
    case 48: c = H(p, mod(floor(uTime * 30.0), 5.0)); break;  // undercranked: choppy
    case 49: {  // void: black, except the brightest things and the edges
        vec3 s = B(p);
        c = s * smoothstep(0.55, 0.85, luma(s)) + pal(uHue + 0.5) * smoothstep(0.1, 0.6, edge(p)) * 1.2;
        break;
    }
    case 50: {  // maximalism: everything at once, in bands
        float band = hash(vec2(floor(p.y * 7.0), floor(uTime * 3.0)));
        c = band < 0.33 ? fxDuplicate(p) : (band < 0.66 ? fxGlitch(p) : fxEcho(p));
        c = max(c, fxHalftone(p) * 0.5);
        break;
    }
    case 51: {  // surreal melt: dripping down like a Dali clock
        float d = pow(noise(vec2(p.x * 18.0, uTime * 0.15)), 3.0) * 0.35 * (0.5 + 0.5 * sin(uTime * 0.4));
        c = S(vec2(p.x + 0.02 * sin(p.y * 10.0 + uTime), p.y + d * (0.5 - p.y))) * vec3(1.05, 1.0, 0.92);
        break;
    }
    case 52: {  // shadow box: cut-paper layers, each casting a shadow on the one behind
        vec2 off = vec2(0.006, -0.008);
        for (int k = 0; k < 4; k++) {
            float th = 0.15 + float(k) * 0.18;
            vec2 q = p * (1.0 - 0.02 * float(k));
            float inL = step(th, luma(S(q))), sh = step(th, luma(S(q + off)));
            c = mix(c * (1.0 - 0.55 * sh), pal(uHue + float(k) * 0.17) * (0.35 + 0.2 * float(k)), inL);
        }
        break;
    }
    case 53: {  // conveyor: strips sliding opposite ways
        float row = floor(p.y * 8.0), f = fract(p.y * 8.0);
        c = B(p + vec2((mod(row, 2.0) * 2.0 - 1.0) * uTime * (0.15 + 0.1 * uBass), 0.0));
        c *= smoothstep(0.0, 0.06, min(f, 1.0 - f));
        break;
    }
    case 54: {  // light painting: a trail of light drawn through the air
        c = B(p) * 0.6;
        float g = 0.0;
        for (int i = 0; i < 60; i++) {
            float s = uTime * 1.2 - float(i) * 0.035, s2 = s - 0.035;
            vec2 a = vec2(asp * 0.35 * sin(s * 1.3 + sin(s * 0.4)), 0.35 * sin(s * 1.9 + 1.0));
            vec2 b = vec2(asp * 0.35 * sin(s2 * 1.3 + sin(s2 * 0.4)), 0.35 * sin(s2 * 1.9 + 1.0));
            g += exp(-pow(sdSeg(p, a, b) * 250.0, 2.0)) * (1.0 - float(i) / 60.0);
        }
        c += pal(uHue + 0.3) * g * 1.5 + vec3(g * 0.5);
        break;
    }
    case 55: {  // jump cuts: a new framing every half second
        float seg = floor(t * 2.5);
        c = B(p / (1.0 + 1.2 * hash(vec2(seg, 1.0))) + (vec2(hash(vec2(seg, 2.0)), hash(vec2(seg, 3.0))) - 0.5) * 0.4);
        break;
    }
    case 56: {  // split diopter: one side close up and sharp, the other far and soft
        float seam = 0.1 * sin(uTime * 0.3);
        c = mix(B(p / 1.6 + vec2(-0.1, 0.0)), blurS(p, 0.02), smoothstep(seam - 0.02, seam + 0.02, p.x));
        break;
    }
    case 57: c = blurS(p, abs(length(p) - (0.5 + 0.5 * sin(t * 0.9)) * 0.5) * 0.05); break;  // rack focus
    case 58: {  // iris: an old film's round shutter opening and closing
        float R = 0.2 + 0.25 * (1.0 + sin(t * 1.2)) + 0.15 * uBeat;
        c = B(p) * smoothstep(R, R - 0.02, length(p)) * vec3(1.05, 0.98, 0.88);
        break;
    }
    case 59: {  // glyph type: giant alien letters, cut out of the picture
        vec2 g = p * 2.2 + vec2(uTime * 0.3, 0.0), id = floor(g);
        vec3 s = B(p);
        c = mix(s * 0.25, 1.0 - clamp(s, 0.0, 1.0) + pal(uHue) * 0.3, glyph(fract(g) - 0.5, hash(id + floor(t / 2.0))));
        break;
    }
    default: c = B(p);
    }
    return max(c, 0.0);
}

// ---- from the trip report
// broken glass: the shard this point is in (its middle, how far from its edge, a random number)
vec4 shards(vec2 x)
{
    vec2 n = floor(x), f = fract(x), mg = vec2(0.0), mr = vec2(0.0);
    float md = 8.0;
    for (int j = -1; j <= 1; j++)
        for (int i = -1; i <= 1; i++) {
            vec2 g = vec2(i, j), o = vec2(hash(n + g + uShatterSeed), hash(n + g + 17.3 + uShatterSeed));
            vec2 r = g + o - f;
            if (dot(r, r) < md) { md = dot(r, r); mr = r; mg = g; }
        }
    md = 8.0;
    for (int j = -2; j <= 2; j++)
        for (int i = -2; i <= 2; i++) {
            vec2 g = mg + vec2(i, j), o = vec2(hash(n + g + uShatterSeed), hash(n + g + 17.3 + uShatterSeed));
            vec2 r = g + o - f;
            if (dot(mr - r, mr - r) > 1e-5) md = min(md, dot(0.5 * (mr + r), normalize(r - mr)));
        }
    return vec4(x + mr, md, hash(n + mg + 3.1));
}
// a person, feet at 0, about 1 tall
float person(vec2 q)
{
    float d = length(q - vec2(0.0, 0.88)) - 0.09;
    d = min(d, sdSeg(q, vec2(0.0, 0.76), vec2(0.0, 0.42)) - 0.07);
    d = min(d, sdSeg(q, vec2(-0.09, 0.72), vec2(-0.2, 0.42)) - 0.03);
    d = min(d, sdSeg(q, vec2(0.09, 0.72), vec2(0.2, 0.42)) - 0.03);
    d = min(d, sdSeg(q, vec2(-0.04, 0.42), vec2(-0.08, 0.0)) - 0.04);
    return min(d, sdSeg(q, vec2(0.04, 0.42), vec2(0.08, 0.0)) - 0.04);
}

// fireworks, the way the trip report has them: no sparks, no smoke, nothing falling. Each is a mandala
// of sacred geometry (rings, a star polygon, flower-of-life petals, spokes; 6, 8 or 12 fold) that
// blooms from a point, and keeps multiplying: every petal tip holds a smaller copy, three deep, so
// it reads as an endless lattice. Hyper-luminous blue, pink and gold with a thin-film shimmer,
// white-hot at the heart, and growing toward you until it fills your sight and passes through you.
vec3 hyperCol(float t)
{
    t = fract(t) * 3.0;
    vec3 blue = vec3(0.15, 0.55, 1.8), pink = vec3(1.8, 0.2, 1.1), gold = vec3(1.9, 1.3, 0.25);
    return t < 1.0 ? mix(blue, pink, t) : (t < 2.0 ? mix(pink, gold, t - 1.0) : mix(gold, blue, t - 2.0));
}
vec3 mandalaBurst(vec2 p, vec4 Bu, float a)
{
    float R = 0.06 * exp(a * 2.6);  // its size: rushing toward you
    vec2 q = (p - Bu.xy) / R;
    if (dot(q, q) > 2.6) return vec3(0.0);  // (nothing of it out here)
    float n = Bu.w < 0.33 ? 6.0 : (Bu.w < 0.66 ? 8.0 : 12.0), seg = 6.2831 / n;
    float spin = fract(Bu.w * 10.0) < 0.5 ? 1.0 : -1.0, scale = R;
    vec2 star = vec2(cos(2.0 * seg), sin(2.0 * seg));
    vec3 c = vec3(0.0);
    for (int l = 0; l < 3; l++) {
        float fl = float(l);
        q = rot(spin * (a * (1.2 + fl * 0.8) + fl * 0.5)) * q;  // each level turning its own way, folding as it blooms
        float r = length(q), fa = abs(mod(atan(q.y, q.x), seg) - seg * 0.5);
        vec2 f = r * vec2(cos(fa), sin(fa));  // folded into one slice: the kaleidoscope
        float d = min(abs(r - 1.0), abs(r - 0.5));
        d = min(d, sdSeg(f, vec2(1.0, 0.0), star));
        d = min(d, abs(length(f - vec2(0.5, 0.0)) - 0.5));
        d = min(d, sdSeg(f, vec2(0.0), vec2(1.0, 0.0)));
        float ds = d * scale;  // how far, on screen
        float stroke = smoothstep(2.2 * px, 0.7 * px, ds) * smoothstep(1.25, 0.95, r);  // a crisp drawn line, not a glow
        vec3 k = hyperCol(Bu.w + fl * 0.33 + r * 0.4 + a * 0.6);
        k *= 0.75 + 0.5 * cos(6.2831 * (fa * 3.0 + r * 2.0 - uTime * 1.5 + vec3(0.0, 0.33, 0.67)));  // iridescent ink
        c += k * stroke * (1.0 - fl * 0.25);
        q = (f - vec2(1.0, 0.0)) / 0.36;  // on to the copy at the petal tip
        scale *= 0.36;
    }
    c += vec3(1.3, 1.25, 1.1) * smoothstep(2.5 * px, 1.0 * px, length(p - Bu.xy)) * exp(-a * 3.0);  // a small inked heart-dot, no halo
    return c * smoothstep(0.0, 0.06, a) * (1.0 - smoothstep(1.2, 3.5, R)) * 0.42;  // crisp, not blown out, so bloom won't re-glow it
}
// the other kind, from the reports: "rapidly multiplying squares shower my vision as they explode in every
// direction: left, right, up, down, then suddenly backwards and forwards", "white strings of light double
// back on themselves, creating cubes in the place of squares", popping like "turquoise-amber-violet
// popcorn kernels". Each square splits in two as it flies (four generations), folds up into a turning
// wireframe cube, and later ones fly at you or away.
vec3 cubeBurst(vec2 p, vec4 Bu, float a)
{
    const vec3 popcorn[3] = vec3[](vec3(0.2, 1.6, 1.4), vec3(1.8, 1.1, 0.2), vec3(1.2, 0.3, 1.8));
    vec3 c = vec3(0.0);
    for (int i = 0; i < 15; i++) {
        float gen = i == 0 ? 0.0 : (i < 3 ? 1.0 : (i < 7 ? 2.0 : 3.0)), born = gen * 0.12;
        if (a < born) break;
        // where it is: its own flight, plus each ancestor's (square i's parent is (i-1)/2)
        vec3 off = vec3(0.0);
        float t = a - born;
        int k = i;
        for (int g = 0; g < 4; g++) {
            float h = hash(vec2(float(k), Bu.w * 37.0)), h2 = hash(vec2(float(k) + 0.5, Bu.w * 53.0));
            int d = int(floor(h * (k < 3 ? 4.0 : 6.0)));  // the first ones in the flat directions, then in and out too
            vec3 dir = d == 0 ? vec3(1, 0, 0) : (d == 1 ? vec3(-1, 0, 0) : (d == 2 ? vec3(0, 1, 0) : (d == 3 ? vec3(0, -1, 0) : (d == 4 ? vec3(0, 0, 1) : vec3(0, 0, -1)))));
            off += dir * t * 0.35 * (0.6 + h2);
            if (k == 0) break;
            k = (k - 1) / 2;
            t = 0.12;
        }
        float persp = 1.0 / max(1.0 - off.z, 0.25);
        vec2 ce = (Bu.xy + off.xy) * persp;
        float s = 0.03 * persp;
        if (length(p - ce) > s * 2.0) continue;
        // the square folding up into a cube, turning
        float fold = smoothstep(0.1, 0.4, a - born), fi = float(i);
        mat2 ry = rot(a * 2.0 + fi * 1.7), rx = rot(a * 1.3 + fi * 0.9);
        vec2 V[8];
        for (int v = 0; v < 8; v++) {
            vec3 q = vec3((v & 1) == 0 ? -1.0 : 1.0, (v & 2) == 0 ? -1.0 : 1.0, ((v & 4) == 0 ? -1.0 : 1.0) * fold);
            q.xz = ry * q.xz;
            q.yz = rx * q.yz;
            V[v] = ce + q.xy * s;
        }
        float dmin = 1.0;
        for (int v = 0; v < 8; v++)
            for (int b = 0; b < 3; b++) {
                int w = v ^ (1 << b);
                if (w > v) dmin = min(dmin, sdSeg(p, V[v], V[w]));
            }
        c += popcorn[int(mod(fi + floor(Bu.w * 30.0), 3.0))] * smoothstep(2.2 * px, 0.7 * px, dmin);  // crisp wireframe stroke, not a glow
    }
    return c * 0.55 * smoothstep(0.0, 0.04, a) * (1.0 - smoothstep(1.1, 1.8, a));
}

// a rose / spirograph bloom: petals traced in line, turning as it grows
vec3 roseBurst(vec2 p, vec4 Bu, float a)
{
    float R = 0.06 * exp(a * 2.5);
    vec2 q = (p - Bu.xy) / R;
    float rr = length(q);
    if (rr > 1.35) return vec3(0.0);
    float spin = fract(Bu.w * 5.0) < 0.5 ? 1.0 : -1.0;
    float th = atan(q.y, q.x) + a * 1.4 * spin;
    float k = floor(3.0 + fract(Bu.w * 13.0) * 4.0);          // 3..6 petals
    vec3 c = vec3(0.0);
    for (int i = 0; i < 2; i++) {
        float rose = (1.0 - 0.45 * float(i)) * abs(cos(k * th));
        float ds = abs(rr - rose) * R;
        c += hyperCol(Bu.w + rr * 0.5 + a * 0.6 + float(i) * 0.2) * smoothstep(2.2 * px, 0.7 * px, ds) * (1.0 - float(i) * 0.3);
    }
    c *= smoothstep(1.35, 1.1, rr);
    c += vec3(1.3, 1.25, 1.1) * smoothstep(2.5 * px, 1.0 * px, length(p - Bu.xy)) * exp(-a * 3.0);
    return c * smoothstep(0.0, 0.06, a) * (1.0 - smoothstep(1.2, 3.5, R)) * 0.42;
}

// concentric rotating polygon rings, like opening portals
vec3 ringBurst(vec2 p, vec4 Bu, float a)
{
    float R = 0.06 * exp(a * 2.5);
    vec2 q = (p - Bu.xy) / R;
    float rr = length(q);
    if (rr > 1.4) return vec3(0.0);
    float n = floor(3.0 + fract(Bu.w * 11.0) * 5.0);          // 3..7 sides
    float seg = 6.2831 / n, dir = fract(Bu.w * 3.0) < 0.5 ? 1.0 : -1.0;
    vec3 c = vec3(0.0);
    for (int i = 0; i < 4; i++) {
        float ri = 0.28 + 0.3 * float(i);
        float ang = atan(q.y, q.x) + a * (0.8 + 0.3 * float(i)) * dir;
        float poly = ri / cos(mod(ang, seg) - seg * 0.5);     // the n-gon's edge at this angle
        c += hyperCol(Bu.w + float(i) * 0.25 + a * 0.5) * smoothstep(2.0 * px, 0.6 * px, abs(rr - poly) * R);
    }
    c *= smoothstep(1.4, 1.15, rr);
    c += vec3(1.3, 1.25, 1.1) * smoothstep(2.5 * px, 1.0 * px, length(p - Bu.xy)) * exp(-a * 3.0);
    return c * smoothstep(0.0, 0.06, a) * (1.0 - smoothstep(1.2, 3.5, R)) * 0.42;
}

// spiral arms unwinding outward
vec3 spiralBurst(vec2 p, vec4 Bu, float a)
{
    float R = 0.06 * exp(a * 2.5);
    vec2 q = (p - Bu.xy) / R;
    float rr = length(q);
    if (rr > 1.4) return vec3(0.0);
    float arms = floor(2.0 + fract(Bu.w * 7.0) * 4.0);        // 2..5 arms
    float dir = fract(Bu.w * 3.0) < 0.5 ? 1.0 : -1.0;
    float ph = (atan(q.y, q.x) * dir * arms + 13.0 * rr - a * 5.0) / 6.2831;  // winding phase
    float d = min(fract(ph), 1.0 - fract(ph));
    float stroke = (1.0 - smoothstep(0.0, fwidth(ph) * 1.3, d)) * smoothstep(1.4, 1.1, rr) * smoothstep(0.05, 0.22, rr);
    vec3 c = hyperCol(Bu.w + rr * 0.8 + a * 0.5) * stroke;
    c += vec3(1.3, 1.25, 1.1) * smoothstep(2.5 * px, 1.0 * px, length(p - Bu.xy)) * exp(-a * 3.0);
    return c * smoothstep(0.0, 0.06, a) * (1.0 - smoothstep(1.2, 3.5, R)) * 0.42;
}

// the watchers: heads at the edges of your vision, alternately outlined black and white, peering in and
// ducking back out when you'd see them ("trying to stay out of my field of vision")
float head(vec2 q) { return min(length(q * vec2(1.0, 0.85)) - 0.5, sdSeg(q, vec2(0.0, -0.4), vec2(0.0, -1.3)) - 0.26); }
// an eye: almond, a glowing iris, a slit pupil, opening and shutting in a blink
vec3 eyeAt(vec2 q, float open, vec3 iris)
{
    float almond = max(length(q - vec2(0.0, -0.75 + 0.35 * (1.0 - open))), length(q - vec2(0.0, 0.75 - 0.35 * (1.0 - open)))) - 1.0;
    if (almond > 0.05) return vec3(0.0);
    float r = length(q), white = smoothstep(0.02, -0.02, almond);
    vec3 c = vec3(0.9, 0.85, 0.95) * white * 0.6;
    c = mix(c, iris * (0.6 + 0.6 * sin(atan(q.y, q.x) * 12.0)), smoothstep(0.34, 0.3, r) * white);
    c *= 1.0 - smoothstep(0.07, 0.04, abs(q.x)) * step(r, 0.3) * 0.95;  // the slit
    return c + iris * exp(-abs(almond) * 40.0) * 0.6;
}

void main()
{
    asp = uRes.x / uRes.y;
    px = 1.0 / uRes.y;
    vec2 p0 = (gl_FragCoord.xy / uRes - 0.5) * vec2(asp, 1.0);
    vec2 p = camera(p0);
    if (uCamTest > 0) {  // (testing: what the camera alone is doing)
        vec2 g = floor(p * 6.0);
        frag = vec4(vec3(mod(g.x + g.y, 2.0)) * 0.8 + 0.1 * vec3(fract(p * 0.5), 0.0), 1.0);
        return;
    }

    // ripples of light from the kicks, bending the picture as they pass
    vec2 bend = vec2(0.0);
    float ring = 0.0;
    for (int i = 0; i < 6; i++) {
        float a = uTime - uRipples[i].z;
        if (a < 0.0 || a > 2.0) continue;
        vec2 d = p0 - uRipples[i].xy;
        float r = length(d), w = r - a * 0.55, env = exp(-w * w * 300.0) * (1.0 - a / 2.0);
        bend += d / (r + 1e-4) * sin(w * 80.0) * 0.012 * env;
        ring += env;
    }
    // shapes under a cloak: you only see them by how the light bends round them
    float rim = 0.0;
    if (uCloak > 0.01) {
        float f = 0.0;
        vec2 gr = vec2(0.0);
        for (int i = 0; i < 3; i++) {
            float fi = float(i);
            vec2 d = p0 - vec2(asp * 0.35 * sin(uTime * 0.13 * (fi + 1.0) + fi), 0.28 * cos(uTime * 0.17 * (fi + 1.0) + fi * 2.0));
            float q = dot(d, d) + 1e-4;
            f += 0.012 / q;
            gr -= 0.024 * d / (q * q);
        }
        float inside = smoothstep(0.85, 1.15, f);
        bend += normalize(gr + 1e-5) * 0.035 * inside * uCloak;
        rim = exp(-pow((f - 1.0) * 5.0, 2.0)) * uCloak;
    }
    p += bend;

    vec3 col = uFx == 0 ? B(p) : effect(p);
    if (rim > 0.0) {  // the cloak's edge: colours split apart, a pink/green shimmer
        vec2 e = vec2(0.006 * rim, 0.0);
        col = mix(col, vec3(S(p + e).r, S(p).g, S(p - e).b), rim);
        col += mix(vec3(1.0, 0.25, 0.7), vec3(0.2, 1.0, 0.5), 0.5 + 0.5 * sin(uTime + p0.x * 4.0)) * rim * 0.35;
    }
    col += pal(uHue + 0.5) * ring * 0.6 + vec3(gRing) * 1.5;

    // the onset (the come-up): what so many reports describe before anything else
    float on = (1.0 - smoothstep(0.5, 1.3, uStage)) * (1.0 - uIntro) * (1.0 - uCalm);  // no onset rainbow over a quieted scene
    if (on > 0.01) {
        vec2 o = vec2(0.006 + 0.01 * uBass, 0.0);  // red/green double images, like 3D glasses
        col = mix(col, vec3(S(p + o).r, S(p - o).g, col.b), 0.55 * on);
        float e = edge(p);
        vec3 rb = 0.5 + 0.5 * cos(6.2831 * (p0.x * 1.5 + p0.y + uTime * 0.4 + vec3(0.0, 0.33, 0.67)));
        col += rb * smoothstep(0.1, 0.5, e) * on * 0.9 * uSpread;  // edges splitting into rainbow fringes (only as the palette widens)...
        col = mix(col, rb * fbm(p * 30.0 + uTime * 2.0) * 2.0, smoothstep(0.5, 1.0, e) * on * uLevel * uSpread);  // ...breaking open into rainbow noise
        col *= 1.0 - smoothstep(0.45, 0.7, e) * 0.85 * on;  // comic-book outlines: a bold black line...
        col += smoothstep(0.8, 1.1, e) * on * 0.8;           // ...around a finer white one
        float band = mod(floor(gl_FragCoord.y / 4.0), 3.0);  // venetian-blind seams tinting red, green, blue in turn
        col *= mix(vec3(1.0), band < 0.5 ? vec3(1.25, 0.85, 0.85) : (band < 1.5 ? vec3(0.85, 1.25, 0.85) : vec3(0.85, 0.85, 1.25)), 0.2 * on * uSpread);
        float cy = fract(uTime / 3.0) * 3.0;  // the room's tint cycling purple, green, red within seconds
        vec3 tint = cy < 1.0 ? mix(vec3(0.8, 0.3, 1.0), vec3(0.3, 1.0, 0.4), cy) : (cy < 2.0 ? mix(vec3(0.3, 1.0, 0.4), vec3(1.0, 0.3, 0.3), cy - 1.0) : mix(vec3(1.0, 0.3, 0.3), vec3(0.8, 0.3, 1.0), cy - 2.0));
        col *= mix(vec3(1.0), tint * 1.3, 0.3 * on * uSpread);  // the room's tint cycle only when the palette is wide
        float corner = smoothstep(0.75, 1.15, length(gl_FragCoord.xy / uRes * 2.0 - 1.0));  // a flickering static veil, thickest in the corners
        col = mix(col, vec3(step(0.5, hash(gl_FragCoord.xy + floor(uTime * 30.0) * 13.1))), corner * on * 0.2 * (0.6 + 0.4 * sin(uTime * 13.0)));
    }
    // the watchers at the edges, and eyes popping open in the periphery
    if (uWatchers > 0.01) {
        for (int i = 0; i < 8; i++) {
            float fi = float(i), th = fi * 0.785 + 0.3 * sin(uTime * 0.1 + fi);
            vec2 dir = vec2(cos(th), sin(th));
            float peek = uWatchers * smoothstep(0.2, 0.9, sin(uTime * 0.6 + fi * 1.7));  // in, then ducking back out
            vec2 pos = dir * vec2(asp * 0.5, 0.5) * (1.12 - 0.22 * peek);
            vec2 q = vec2(dot(p0 - pos, vec2(-dir.y, dir.x)), dot(p0 - pos, -dir)) / 0.12;  // turned to look in toward the middle
            float line = exp(-abs(head(q)) * 0.12 * 400.0) * peek;
            if (mod(fi, 2.0) < 0.5) col *= 1.0 - line * 0.9;
            else col += line * 1.1;
        }
        for (int i = 0; i < 6; i++) {
            float fi = float(i), slot = floor(uTime * 0.7 + fi * 0.37);
            vec2 at = vec2(hash(vec2(fi, slot)) - 0.5, hash(vec2(fi + 9.0, slot)) - 0.5) * vec2(asp, 1.0);
            if (length(at / vec2(asp, 1.0)) < 0.3) continue;  // only out in the periphery
            float ph = fract(uTime * 0.7 + fi * 0.37), open = floor(min(1.0, sin(ph * PI) * 1.6) * 4.0) / 4.0;  // popping open in digital steps
            col += eyeAt((p0 - at) / 0.045, open, pal(uHue + fi * 0.2) * 1.5) * uWatchers;
        }
    }

    // breaking through, on a big drop, one of three ways. The shatter: the picture breaks like glass and
    // the pieces fly at you
    if (uShatterT >= 0.0 && uShatterT < 2.5 && uBreakKind < 0.5) {
        float t = uShatterT;
        vec4 sh = shards(p0 * 5.0);
        vec2 cen = sh.xy / 5.0;
        float fly = t * t * (0.5 + sh.w), gap = 0.02 + fly * 0.45;
        vec2 q = cen - normalize(cen + 1e-4) * fly * 0.3 + rot(fly * (sh.w - 0.5) * 6.0) * (p0 - cen) / (1.0 + fly);
        float piece = smoothstep(gap, gap + 0.02, sh.z) * (1.0 - smoothstep(1.6, 2.5, t));
        vec3 glass = F(q) * (1.0 + 0.6 * exp(-t * 3.0));
        glass += mix(vec3(1.0, 0.25, 0.7), vec3(0.2, 1.0, 0.5), sh.w) * smoothstep(gap + 0.06, gap, sh.z) * 1.5;  // bright edges
        col = mix(col, glass, piece);
        col += pal(uHue + sh.w) * (1.0 - piece) * exp(-t * 1.5) * 0.5;  // colour pouring through the cracks
        col += smoothstep(0.03, 0.0, sh.z) * exp(-t * 10.0) * 2.0;      // the crack itself, white
    }
    // white light: brightest in the middle, darkening toward the edges, then the world reassembles out of it
    if (uShatterT >= 0.0 && uShatterT < 2.5 && uBreakKind > 0.5 && uBreakKind < 1.5) {
        float t = uShatterT, back = clamp((t - 0.8) / 1.4 * 1.6 - noise(p0 * 14.0) * 0.6, 0.0, 1.0);
        col = mix(col, vec3(1.3, 1.25, 1.2) * max(1.4 - length(p0) * 1.3, 0.0), smoothstep(0.0, 0.35, t) * (1.0 - back));
    }
    // the curtain: the old picture torn down the middle, its halves pulled apart, rippling like fabric
    if (uShatterT >= 0.0 && uShatterT < 2.5 && uBreakKind > 1.5) {
        float t = uShatterT, open = smoothstep(0.15, 1.8, t) * 0.55 * asp;
        float x = p0.x - 0.03 * (noise(vec2(p0.y * 20.0, uShatterSeed)) - 0.5) - 0.008 * sin(p0.y * 90.0);
        float side = x < 0.0 ? -1.0 : 1.0;
        vec2 q = p0 - vec2(side * open, 0.0);
        q.y += 0.012 * sin(q.x * 30.0 + t * 6.0) * smoothstep(0.0, 0.5, open);
        float cloth = step(open, abs(x));
        vec3 fab = F(q) * (0.75 + 0.25 * sin(q.x * 60.0 + t * 4.0));  // folds
        col = mix(col, fab, cloth);
        col += vec3(1.0, 0.95, 0.9) * exp(-abs(abs(x) - open) * 300.0) * 1.2 * (1.0 - smoothstep(1.8, 2.5, t));  // the torn edge's fibres
    }
    // ...and the outlines of figures, glimpsed in the chaos
    if (uFigures > 0.01) {
        float g = 0.0;
        for (int i = 0; i < 5; i++) {
            float fi = float(i), h = 0.42 + 0.12 * hash(vec2(fi, uShatterSeed));
            vec2 q = (p0 - vec2((fi - 2.0) * 0.16 * asp + 0.05 * sin(uTime + fi), -0.48)) / h;
            q.x += 0.03 * sin(q.y * 8.0 + uTime * 3.0 + fi);
            g = max(g, exp(-abs(person(q) * h) * 160.0));
        }
        col += mix(vec3(0.9, 1.0, 1.0), pal(uHue + 0.3), 0.4) * g * uFigures * (0.6 + 0.4 * noise(p0 * 20.0 + uTime * 4.0)) * 1.4;
    }
    // fireworks
    vec3 fw = vec3(0.0);
    for (int b = 0; b < 16; b++) {
        float a = uTime - uBursts[b].z;
        a = floor(a * 18.0) / 18.0;  // moving in digital steps, "animated at 99 frames per second"
        if (a >= 0.0 && a < 1.8) {
            float w = uBursts[b].w;
            vec4 B = vec4(uBursts[b].xyz, fract(w * 7.919));  // a per-burst variation for whichever type
            if      (w < 0.28) fw += mandalaBurst(p0, B, a);
            else if (w < 0.46) fw += cubeBurst(p0, B, a);
            else if (w < 0.64) fw += roseBurst(p0, B, a);
            else if (w < 0.82) fw += ringBurst(p0, B, a);
            else               fw += spiralBurst(p0, B, a);
        }
    }
    // keep the bursts in the same drifting band as everything else: calm = the scene's hue, energy = their own blue/pink/gold
    fw = mix(dot(fw, vec3(0.333)) * pal(uHue) * 2.0, fw, clamp(uSpread * 1.8, 0.0, 1.0));
    col += fw;
    if (any(isnan(col)) || any(isinf(col))) col = vec3(0.0);
    frag = vec4(clamp(col, 0.0, 64.0), 1.0);
}
)";
