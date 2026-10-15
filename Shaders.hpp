// The pictures, as GPU shader programs (GLSL). Every pixel is worked out
// from scratch every frame, from the time and what the music is doing.
//
// The scene shader draws the trip's four stages, crossfading as it goes:
//   0 COME-UP        breathing colour, a faint lattice appearing
//   1 CHRYSANTHEMUM  a kaleidoscope mandala falling inward forever, with a
//                    hyperbolic mosaic (tiles that shrink toward the edge of a disc) at its heart
//   2 BREAKTHROUGH   flying down a tunnel walled with alien glyphs, a serpent coiling through it
//   3 HYPERSPACE     inside a dome tiled hyperbolically, flowing past you, with an entity in front
// Over every stage lies one of six repeating patterns (flower of life, truchet weave, op-art,
// moire, a Julia fractal, a honeycomb), crossfading to the next on drops.
// ...then mixes in the last frame, zoomed and hue-shifted, so everything melts and trails.
// Wireframes (Wire.hpp) are drawn on top with the line shader.
// FX mirrors the picture (left/right, four ways, kaleidoscopes), traces neon around every edge
// and pushes the contrast; BRIGHT and BLUR make the glow (bloom); PRESENT puts it all on screen
// with a little colour fringing and grain.
#pragma once

static const char* const kVertex = R"(#version 330 core
// one triangle that covers the whole screen
void main()
{
    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)";

static const char* const kScene = R"(#version 330 core
out vec4 frag;
uniform vec2 uRes;
uniform float uTime, uBass, uMid, uHigh, uLevel, uBeat, uStage, uEntityAmt, uMood, uHue, uZoom, uSpin;
uniform int uEntity, uPatA, uPatB;
uniform float uPatT, uPatAmt;
uniform float uGridTurn, uGridFocus;
uniform vec2 uGridPos;
uniform sampler2D uPrev;
uniform sampler2D uMedia;   // a dropped picture / video (none: black)
uniform float uMediaAmt, uMediaAge;  // how much of it, and how long since it was dropped
uniform float uIntro;  // 1 just after a drop: the picture itself, before the trip takes it
uniform vec3 uSeedPal[3];  // a dropped picture's three main colours...
uniform float uSeedMix;     // ...and how much they colour everything

#define PI 3.14159265

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
    for (int i = 0; i < 5; i++) { s += a * noise(p); p = rot(0.5) * p * 2.03 + 11.0; a *= 0.5; }
    return s;
}
// a dropped picture's colours, going round
vec3 seedRamp(float t)
{
    t = fract(t) * 3.0;
    vec3 c = t < 1.0 ? mix(uSeedPal[0], uSeedPal[1], t) : (t < 2.0 ? mix(uSeedPal[1], uSeedPal[2], t - 1.0) : mix(uSeedPal[2], uSeedPal[0], t - 2.0));
    return c / max(max(c.r, c.g), max(c.b, 0.08)) * 0.95;  // as bright as it'll go
}
// colours: candy rainbow (leaning pink and green) when the music is bright, violet / teal / crimson when it's dark
vec3 pal(float t)
{
    vec3 bright = 0.5 + 0.5 * cos(6.2831 * (t + vec3(0.0, 0.33, 0.67)));
    bright = mix(bright, mix(vec3(1.0, 0.2, 0.65), vec3(0.2, 1.0, 0.5), 0.5 + 0.5 * cos(6.2831 * t)), 0.4);  // pulled toward pink and green
    vec3 dark = vec3(0.32, 0.18, 0.42) + vec3(0.38, 0.32, 0.45) * cos(6.2831 * (t + vec3(0.62, 0.42, 0.18)));
    vec3 c = mix(dark, bright, uMood);
    return uSeedMix > 0.0 ? mix(c, seedRamp(t) * mix(0.6, 1.0, uMood), 0.65 * uSeedMix) : c;
}
vec3 hueShift(vec3 c, float a)
{
    const vec3 k = vec3(0.57735);
    float ca = cos(a * 6.2831), sa = sin(a * 6.2831);
    return c * ca + cross(k, c) * sa + k * dot(k, c) * (1.0 - ca);
}
float sdSeg(vec2 p, vec2 a, vec2 b)
{
    vec2 pa = p - a, ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba * h);
}
vec4 MT(vec2 uv) { return texture(uMedia, vec2(uv.x, 1.0 - uv.y)); }
vec3 M(vec2 uv) { return MT(uv).rgb; }
float MA(vec2 uv) { return MT(uv).a; }  // 0 where it had no background (cut out, or see-through)  // the dropped picture (off its edges, mirrored)
float smin(float a, float b, float k) { float h = clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0); return mix(b, a, h) - k * h * (1.0 - h); }

// ---- complex numbers, for moving around the hyperbolic disc
vec2 cmul(vec2 a, vec2 b) { return vec2(a.x * b.x - a.y * b.y, a.x * b.y + a.y * b.x); }
vec2 cdiv(vec2 a, vec2 b) { return vec2(a.x * b.x + a.y * b.y, a.y * b.x - a.x * b.y) / dot(b, b); }
vec2 mobius(vec2 z, vec2 w) { return cdiv(z + w, vec2(1.0, 0.0) + cmul(vec2(w.x, -w.y), z)); }

// A {P,Q} hyperbolic tiling of the unit disc: P-sided tiles, Q meeting at each corner.
// Folds the point into one small triangle by mirroring; returns how many folds it took
// (which tile), how close it is to the tile's edge, and where it landed.
vec3 hyper(vec2 z, float P, float Q)
{
    float a = PI / P, b = PI / Q;
    float sa = sin(a), cb = cos(b);
    float den = sqrt(cb * cb - sa * sa);
    float r = sa / den, d = cb / den;
    vec2 c = vec2(d, 0.0), n = vec2(-sa, cos(a));
    float count = 0.0;
    for (int i = 0; i < 48; i++) {
        bool moved = false;
        if (z.y < 0.0) { z.y = -z.y; moved = true; }
        float k = dot(z, n);
        if (k > 0.0) { z -= 2.0 * k * n; moved = true; }
        vec2 dz = z - c;
        float l = dot(dz, dz);
        if (l < r * r) { z = c + r * r * dz / l; count += 1.0; moved = true; }
        if (!moved) break;
    }
    return vec3(count, abs(length(z - c) - r), length(z));
}

// alien glyphs: each cell of a grid draws a few strokes, picked by its own random number
float glyph(vec2 f, float h)
{
    const vec2 G[9] = vec2[](vec2(-0.3, -0.3), vec2(0.0, -0.3), vec2(0.3, -0.3), vec2(-0.3, 0.0), vec2(0.0, 0.0),
                             vec2(0.3, 0.0), vec2(-0.3, 0.3), vec2(0.0, 0.3), vec2(0.3, 0.3));
    const ivec2 S[12] = ivec2[](ivec2(0, 1), ivec2(1, 2), ivec2(3, 4), ivec2(4, 5), ivec2(6, 7), ivec2(7, 8),
                               ivec2(0, 3), ivec2(3, 6), ivec2(2, 5), ivec2(5, 8), ivec2(0, 4), ivec2(4, 8));
    float bits = floor(h * 4096.0), d = 1.0;
    for (int i = 0; i < 12; i++) {
        if (mod(floor(bits / exp2(float(i))), 2.0) > 0.5) d = min(d, sdSeg(f, G[S[i].x], G[S[i].y]));
    }
    return smoothstep(0.07, 0.0, d);
}

// the grid: an endless floor and ceiling of lines in a black void, sharpening into focus as the trip
// deepens, the floor checkered like an old soda shop. It turns square corners (uGridTurn).
vec3 voidGrid(vec2 p)
{
    vec3 rd = normalize(vec3(p, 1.1));
    rd.xz = rot(uGridTurn) * rd.xz;
    float t = 1.0 / max(abs(rd.y), 1e-3), floorSide = step(rd.y, 0.0);
    vec2 h = rd.xz * t + uGridPos;
    h += 0.25 * vec2(noise(h * 0.4 + uTime * 0.2), noise(h * 0.4 - uTime * 0.2 + 4.0));  // warping, like a film of oil
    vec2 g = abs(fract(h) - 0.5);
    float d = 0.5 - max(g.x, g.y);  // how far to the nearest line
    float w = mix(0.2, 0.025, uGridFocus) + fwidth(d);
    vec3 lc = mix(vec3(0.2, 1.0, 0.5), vec3(1.0, 0.2, 0.65), floorSide);  // green above, pink below
    lc = mix(lc, 0.5 + 0.5 * cos(6.2831 * (h.x * 0.08 + h.y * 0.05 + uTime * 0.3 + vec3(0.0, 0.33, 0.67))), 0.4);  // with an oil-film rainbow
    vec3 c = lc * smoothstep(w, 0.0, d) * 1.2 + vec3(1.0) * smoothstep(w * 0.35, 0.0, d) * 0.6;  // chrome: a white core
    c += floorSide * mod(floor(h.x) + floor(h.y), 2.0) * vec3(0.9, 0.85, 0.95) * 0.06 * uGridFocus;
    // neon blocks popping in and out, digitally, like Tetris pieces
    float pick = hash(floor(h) + floor(uTime * 6.0 + hash(floor(h)) * 6.0) * 0.37);
    if (pick > 0.965) {
        float k = fract(pick * 41.0);
        vec3 neon = k < 0.25 ? vec3(1.0, 0.15, 0.2) : (k < 0.5 ? vec3(0.2, 1.0, 0.3) : (k < 0.75 ? vec3(0.2, 0.4, 1.0) : vec3(1.0, 0.9, 0.2)));
        c += neon * (0.5 + 0.5 * smoothstep(0.5, 0.3, max(g.x, g.y))) * 0.8 * uGridFocus;
    }
    c *= 1.0 + 1.5 * uBeat * exp(-abs(fract(t * 0.15 - uTime * 0.6) - 0.5) * 10.0);  // a surge along the lines on the kick
    return c * exp(-t * 0.09) * (0.6 + 0.6 * uLevel);
}

// ---- 0: COME-UP
vec3 comeup(vec2 p)
{
    vec2 q = p * 1.6;
    float t = uTime * 0.05;
    vec2 w = vec2(fbm(q + t), fbm(q - t + 3.0));
    float f = fbm(q + 2.0 * w + uBass * 0.3);
    float r = length(p);
    float breath = 0.5 + 0.5 * sin(r * 6.0 - uTime * 0.8 + uBass);  // soft rings, breathing out from the middle
    vec3 c = pal(f * 1.4 + r * 0.4 + uHue) * (0.3 + 0.7 * f) * (0.55 + 0.45 * breath) * (0.7 + 0.6 * uLevel);
    c += pal(uHue + 0.2) * 0.25 * exp(-r * 3.0) * (1.0 + uBass);  // a warm glow at the centre
    // the lattice: three sets of lines, coming through as the trip deepens
    float g = 0.0;
    vec2 d1 = vec2(1, 0), d2 = vec2(0.5, 0.866), d3 = vec2(-0.5, 0.866);
    vec2 lq = rot(uSpin * 0.3) * q * 5.0;
    for (int i = 0; i < 3; i++) {
        vec2 dir = i == 0 ? d1 : (i == 1 ? d2 : d3);
        float v = fract(dot(lq, dir) + uTime * 0.03);
        g = max(g, smoothstep(0.05, 0.0, min(v, 1.0 - v)));
    }
    c += pal(uHue + 0.5) * g * (0.3 + 0.6 * uHigh) * clamp(uStage * 2.0 + 0.3, 0.3, 1.0);
    if (uMediaAmt > 0.0) {
        // a dropped picture: nearly as it is at first, then breathing, warping and melting into the rest
        float melt = 1.0 - exp(-uMediaAge * 0.12);
        vec2 w = vec2(fbm(p * 2.0 + uTime * 0.1), fbm(p * 2.0 - uTime * 0.1 + 7.0)) - 0.5;
        vec2 uv = (p * (1.0 - 0.04 * sin(uTime * 0.8) * melt) + w * (0.01 + 0.12 * melt)) / vec2(uRes.x / uRes.y, 1.0) + 0.5;
        c = mix(c, M(uv) * (1.0 + 0.4 * uBass) + c * 0.3 * melt, uMediaAmt * (1.0 - 0.6 * melt) * MA(uv));
    }
    return c * 0.85 + voidGrid(p) * (0.15 + 0.45 * clamp(uStage, 0.0, 1.0));  // the grid comes as you go deeper
}

// ---- 1: CHRYSANTHEMUM
// "standing waves of colour, like fine powder patterns on a metal plate vibrating in a sound field": a
// Chladni plate, its mode set by the music (bass and highs slide it between patterns), folded into the
// kaleidoscope so it's a mandala. The powder gathers where the plate stays still.
vec3 chladni(vec2 f)
{
    vec2 q = f * 2.4;
    float n = 2.0 + uBass * 3.0, m = 3.0 + uHigh * 4.0 + uMid * 2.0;
    float v = cos(n * PI * q.x) * cos(m * PI * q.y) - cos(m * PI * q.x) * cos(n * PI * q.y);
    return pal(uHue + 0.15 + v * 0.2) * smoothstep(0.12, 0.0, abs(v)) * (0.4 + 0.8 * uLevel);
}
vec3 chrysanthemum(vec2 p)
{
    float r = length(p) + 1e-4, a = atan(p.y, p.x);
    float N = 12.0, seg = 2.0 * PI / N;
    float ka = abs(mod(a + uSpin, seg) - seg * 0.5);   // folded into one slice: a kaleidoscope
    float lr = log(r);                                 // log of the distance: rings that fall inward forever
    vec2 k = vec2(lr * 2.0 - uTime * 0.35, ka * 3.0);
    float f = fbm(k * 2.0 + uMid * 0.2);
    float rings = 0.5 + 0.5 * sin(lr * 9.0 - uTime * 1.3 - uBass * 1.5 + f * 3.0);
    float petals = 0.5 + 0.5 * cos(ka * N + lr * 4.0 - uTime * 0.6);
    float v = rings * petals;
    vec3 c = pal(v * 0.6 + lr * 0.25 + uHue) * (0.2 + 1.1 * v);
    // fine glowing filigree along the petal edges
    float fil = abs(sin(lr * 14.0 + ka * 7.0 - uTime * 1.1));
    c += pal(uHue + 0.5) * smoothstep(0.08, 0.0, fil) * (0.4 + 0.8 * uHigh);
    c += chladni(r * vec2(cos(ka), sin(ka))) * 0.8;
    c = mix(c, M(vec2(ka / seg, lr * 0.35 - uTime * 0.05)) * (0.25 + 1.1 * v), 0.6 * uMediaAmt * MA(vec2(ka / seg, lr * 0.35 - uTime * 0.05)));  // the picture, folded into petals
    // the heart: a hyperbolic mosaic, drifting
    vec2 z = p / 0.42;
    float zl = length(z);
    if (zl < 1.0) {
        z = mobius(rot(uSpin * 0.7) * z, 0.35 * vec2(cos(uTime * 0.11), sin(uTime * 0.13)));
        vec3 h = hyper(z, 7.0, 3.0);
        vec3 tile = pal(h.x * 0.13 + uHue + 0.2) * (0.45 + 0.4 * mod(h.x, 2.0));
        tile += pal(uHue + 0.7) * smoothstep(0.03, 0.0, h.y) * 1.2;
        tile = mix(tile, M(z * 0.5 + 0.5) * (0.5 + 0.5 * mod(h.x, 2.0)), 0.5 * uMediaAmt * MA(z * 0.5 + 0.5));  // and tiled through the heart
        tile *= 0.4 + 0.8 * smoothstep(1.0, 0.0, h.z);
        c = mix(c, tile, smoothstep(1.0, 0.9, zl));
    }
    return c * (0.6 + 0.6 * uLevel);
}

// ---- 2: BREAKTHROUGH
// the far wall: a tunnel of alien glyphs, a serpent coiling through it
vec3 glyphWall(vec2 p)
{
    float r = length(p) + 1e-3, a = atan(p.y, p.x);
    vec2 t = vec2(a / PI * 3.0, 0.35 / r + uZoom);  // walls unrolled: round the tunnel, and along it
    vec2 g = t * 4.0;
    vec2 id = floor(g), f = fract(g) - 0.5;
    float h = hash(id);
    float gl = glyph(f, h) * (0.5 + 0.5 * sin(uTime * 3.0 + h * 20.0 + uHigh * 4.0));
    float cell = smoothstep(0.5, 0.42, max(abs(f.x), abs(f.y)));
    vec3 c = pal(h * 0.3 + t.y * 0.05 + uHue) * (0.12 + 0.2 * cell) + pal(uHue + h) * gl * (0.6 + uHigh);
    c = mix(c, M(vec2(t.x / 6.0 + 0.5, t.y * 0.15)) * (0.5 + 0.6 * gl + 0.3 * cell), 0.55 * uMediaAmt * MA(vec2(t.x / 6.0 + 0.5, t.y * 0.15)));  // the picture papering the walls
    // the serpent, coiling along the tunnel
    float path = 1.4 * sin(t.y * 1.1 + uTime * 0.6) + 0.6 * sin(t.y * 2.3);
    float dx = abs(mod(t.x - path + 3.0, 6.0) - 3.0);
    float body = smoothstep(0.42, 0.3, dx);
    if (body > 0.0) {
        float scales = 0.5 + 0.5 * sin((t.y * 18.0) + abs(dx) * 12.0) * sin(t.x * 10.0);
        vec3 skin = mix(vec3(0.05, 0.6, 0.35), vec3(1.0, 0.75, 0.2), scales) * (1.0 - dx * 1.5);
        c = mix(c, skin * (0.6 + uBass * 0.5), body * 0.9);
    }
    c *= smoothstep(0.0, 0.5, r * 2.0);              // walls fade into the distance...
    c += vec3(1.0, 0.92, 0.85) * 0.018 / (r + 0.02) * (0.6 + uBass);  // ...where the light is
    return c;
}

// in front of it, a tube of pipes you fly down: pipes on a 3D grid, running straight for a while,
// turning square corners, balls on the bends, nothing in the middle so there's a way through
float h3(vec3 p) { p = fract(p * vec3(0.1031, 0.1030, 0.0973)); p += dot(p, p.yzx + 33.33); return fract((p.x + p.y) * p.z); }
bool core(vec3 n) { float m = max(abs(n.x), abs(n.y)); return m < 1.5 || m > 5.5; }  // the way through, and the outside
// is there a pipe from grid point n one step along axis a? returns its colour pick + 1, or 0
float pipeOn(vec3 n, int a)
{
    vec3 m = n;
    m[a] += 1.0;
    if (core(n) || core(m)) return 0.0;
    vec3 o = n;
    o[a] = float(a) * 17.0 + 0.5;                  // the whole line this step is on
    float L = 2.0 + floor(h3(o) * 7.0);            // which comes in runs this long
    vec3 k = o;
    k[a] += floor((n[a] + floor(h3(o + 5.0) * L)) / L) * 7.13;
    return h3(k + 3.1) > 0.74 ? 1.0 + h3(k + 9.7) : 0.0;
}
const float PR = 0.13;  // pipe radius
// distance to the nearest pipe; id = its colour pick, along = how far along it
float pipes(vec3 q, out float id, out float along)
{
    vec3 n = floor(q + 0.5), f = q - n;
    float d = 1e9;
    float side = max(abs(q.x), abs(q.y));
    if (side < 1.3) return 2.0 - PR * 1.45 - side;  // the empty middle: jump straight to the nearest pipes
    id = 0.0;
    along = 0.0;
    int deg = 0, straight = 0;
    for (int a = 0; a < 3; a++) {
        int here = 0;
        for (int s = 0; s < 2; s++) {
            vec3 from = n;
            if (s == 1) from[a] -= 1.0;
            float on = pipeOn(from, a);
            if (on < 0.5) continue;
            deg++;
            here++;
            vec3 g = f;
            g[a] -= s == 0 ? clamp(f[a], 0.0, 0.5) : clamp(f[a], -0.5, 0.0);
            float e = length(g) - PR;
            if (e < d) { d = e; id = on; along = q[a]; }
        }
        if (here == 2) straight = 1;
    }
    if (deg > 0 && !(deg == 2 && straight == 1)) d = min(d, length(f) - PR * 1.45);  // a ball on bends and ends
    // never step past the next cell's pipes
    return min(d, 0.5 - max(abs(f.x), max(abs(f.y), abs(f.z))) + 0.5 - PR * 1.45);
}

vec3 tunnel(vec2 p)
{
    vec3 back = glyphWall(p) * 0.25;
    vec3 ro = vec3(0.3 * sin(uTime * 0.4), 0.3 * cos(uTime * 0.31), uZoom * 8.0);
    vec3 rd = normalize(vec3(p, 0.8));
    rd.xy = rot(uSpin * 0.6 + 0.25 * sin(uTime * 0.13)) * rd.xy;  // banking as it goes
    float t = 0.0, id, along;
    bool hit = false;
    for (int i = 0; i < 64; i++) {
        float d = pipes(ro + rd * t, id, along);
        if (d < 0.002 + 0.006 * t) { hit = true; break; }
        t += d;
        vec3 at = ro + rd * t;
        if (t > 32.0 || max(abs(at.x), abs(at.y)) > 6.0) break;  // out past the pipes
    }
    if (!hit) return back;
    vec3 q = ro + rd * t;
    float i0, a0;
    vec2 e = vec2(0.002, -0.002);
    vec3 nrm = normalize(e.xyy * pipes(q + e.xyy, i0, a0) + e.yyx * pipes(q + e.yyx, i0, a0) +
                         e.yxy * pipes(q + e.yxy, i0, a0) + e.xxx * pipes(q + e.xxx, i0, a0));
    // glossy plastic lit from the camera, like the old screensaver, in this moment's colours
    vec3 base = pal(floor(id * 5.0) * 0.2 + uHue);
    base = mix(base, M(vec2(id, along * 0.03)) * 1.2, 0.6 * uMediaAmt * MA(vec2(id, along * 0.03)));  // painted with the picture
    float diff = max(dot(nrm, -rd), 0.0);
    float spec = pow(max(dot(reflect(rd, nrm), -rd), 0.0), 40.0);
    float rim = pow(1.0 - diff, 3.0);
    vec3 c = base * (0.12 + 0.85 * diff) + vec3(spec) * 0.9 + pal(uHue + 0.5) * rim * 0.5;
    // light pulses running along the pipes, faster and brighter with the highs
    float pulse = smoothstep(0.82, 1.0, sin(along * 1.3 - uTime * (5.0 + 6.0 * uHigh) + id * 40.0));
    c += pal(uHue + id) * pulse * (0.4 + 1.2 * uHigh + uBeat);
    c *= 0.75 + 0.5 * uLevel;
    return mix(back, c, exp(-t * 0.07));  // far pipes sink into the glyph wall
}

// ---- 3: HYPERSPACE
vec3 dome(vec2 p)
{
    vec2 z = p / 0.5;
    float zl = length(z);
    vec3 c = vec3(0.0);
    // outside the dome: the grid, and a dim sea of glyphs
    vec2 g = rot(uSpin * 0.2) * p * 9.0;
    c += pal(uHue + 0.3) * glyph(fract(g) - 0.5, hash(floor(g))) * 0.12 * (0.5 + uHigh) + voidGrid(p);
    if (zl < 1.0) {
        // the tiles flow past: the disc moved through hyperbolic space
        z = mobius(z, 0.55 * vec2(sin(uZoom * 0.2), cos(uZoom * 0.17)));
        z = rot(uSpin) * z;
        vec3 h = hyper(z, 8.0, 3.0);
        float k = mod(h.x, 3.0);
        vec3 tile = pal(h.x * 0.09 + uHue + k * 0.15) * (0.3 + 0.35 * k);
        // inside each tile: a small mandala of its own
        tile = mix(tile, M(z * 0.5 + 0.5) * (0.4 + 0.4 * k), 0.5 * uMediaAmt * MA(z * 0.5 + 0.5));  // a dropped picture, tiled across the dome
        float m = 0.5 + 0.5 * cos(h.z * 40.0 - uTime * 2.0);
        tile *= 0.5 + 0.7 * m;
        tile += pal(uHue + 0.5) * smoothstep(0.025, 0.0, h.y) * (1.0 + uHigh);
        c = mix(c, tile * (0.6 + 0.6 * uLevel), smoothstep(1.0, 0.97, zl));
    }
    return c;
}

// ---- repeating patterns
// a hexagonal grid: the point relative to its cell's centre (xy), and the centre (zw)
vec4 hexCell(vec2 p)
{
    const vec2 r = vec2(1.0, 1.7320508);
    vec2 a = mod(p, r) - r * 0.5, b = mod(p - r * 0.5, r) - r * 0.5;
    vec2 g = dot(a, a) < dot(b, b) ? a : b;
    return vec4(g, p - g);
}
float glow(float d, float w) { return w / (abs(d) + w); }

vec3 pattern(vec2 p, int id)
{
    float t = uTime;
    if (id == 0) {  // flower of life: circles on every point of a triangular lattice, each through its neighbours
        vec2 q = rot(uSpin * 0.5) * p * (5.0 + 0.8 * uBass);
        vec4 h = hexCell(q);
        float d = abs(length(h.xy) - 1.0);
        for (int i = 0; i < 6; i++) {
            float a = float(i) * PI / 3.0;
            d = min(d, abs(length(h.xy - vec2(cos(a), sin(a))) - 1.0));
        }
        float l = glow(d, 0.02) * (0.6 + uHigh);
        return pal(uHue + length(h.zw) * 0.05 - t * 0.05) * l;
    }
    if (id == 1) {  // truchet weave: each tile two quarter-circle bands, turned at random -> endless knotwork
        vec2 q = rot(0.2 * sin(t * 0.1)) * p * 7.0 + vec2(0.0, t * 0.3);
        vec2 cid = floor(q), f = fract(q) - 0.5;
        if (hash(cid) > 0.5) f.x = -f.x;
        float d1 = length(f - 0.5) - 0.5, d2 = length(f + 0.5) - 0.5;
        float d = min(abs(d1), abs(d2));
        float band = smoothstep(0.16, 0.12, d) - smoothstep(0.06, 0.02, d) * 0.7;
        float flow = 0.5 + 0.5 * sin((abs(d1) < abs(d2) ? atan(f.y - 0.5, f.x - 0.5) : atan(f.y + 0.5, f.x + 0.5)) * 8.0 - t * 4.0);
        return pal(uHue + hash(cid) * 0.2 + 0.3) * band * (0.5 + 0.8 * flow) * (0.7 + uMid);
    }
    if (id == 2) {  // op-art: black and white stripes, warped, falling inward
        float r = length(p) + 1e-4, a = atan(p.y, p.x);
        float v = sin(a * 10.0 + uSpin * 3.0 + sin(log(r) * 3.0)) * sin(log(r) * 14.0 - t * 2.0 - uBass * 2.0);
        float bw = smoothstep(-0.04, 0.04, v);
        return bw * mix(vec3(0.75), pal(uHue + r), 0.55) * (0.55 + 0.3 * uLevel);  // tinted, not pure white: it sits on bright scenes
    }
    if (id == 3) {  // moire: two sets of rings, drifting apart and together
        vec2 c = vec2(0.18 * sin(t * 0.31), 0.12 * cos(t * 0.43)) * (1.0 + uBass);
        float v = sin(length(p - c) * 110.0) * sin(length(p + c) * 110.0);
        return pal(uHue + v * 0.15 + length(p)) * smoothstep(0.0, 0.3, v) * 1.1;
    }
    if (id == 4) {  // a Julia fractal, folded six ways; its shape follows the bass
        float r = length(p), a = atan(p.y, p.x) + uSpin;
        float seg = PI / 3.0;
        a = abs(mod(a, seg) - seg * 0.5);
        vec2 z = r * vec2(cos(a), sin(a)) * 2.4;
        vec2 c = vec2(-0.79 + 0.06 * sin(t * 0.13), 0.15 + 0.05 * cos(t * 0.11)) + vec2(0.0, uBass * 0.025);
        float n = 0.0;
        for (int i = 0; i < 56; i++) {
            z = cmul(z, z) + c;
            if (dot(z, z) > 16.0) break;
            n += 1.0;
        }
        if (n >= 56.0) return pal(uHue + 0.5) * 0.08;
        float sn = n - log2(log2(dot(z, z))) + 4.0;
        return pal(sn * 0.04 + uHue) * (0.3 + 0.9 * smoothstep(0.0, 30.0, sn));
    }
    // id 5: the honeycomb: glowing hex cells, some flashing on the beat
    vec2 q = rot(uSpin * 0.3) * p * 9.0;
    vec4 h = hexCell(q);
    vec2 g = abs(h.xy);
    float e = max(dot(g, normalize(vec2(1.0, 1.7320508))), g.x);  // distance from the cell's centre, hexagonally
    float edge = glow(e - 0.5, 0.015);
    float cell = hash(h.zw + floor(t * 2.0));
    float flash = step(1.0 - 0.35 * uBeat - 0.05, cell);
    vec3 fill = pal(uHue + cell * 0.3) * (0.15 + 0.9 * flash) * smoothstep(0.5, 0.2, e);
    return fill + pal(uHue + 0.5) * edge * 0.6;
}

// ---- entities

// an eye: almond, with a patterned iris and a slit pupil; looks toward `look`
vec4 eye(vec2 q, float s, float open, vec2 look, float seed)
{
    q /= s;
    float o = max(open, 0.03);
    float lens = length(vec2(q.x, q.y / (o * 0.55)));
    float inside = smoothstep(1.0, 0.92, lens) * step(abs(q.x), 1.0);
    float lid = smoothstep(0.1, 0.0, abs(lens - 1.0)) * step(abs(q.x), 1.05);
    vec2 iq = q - look * 0.35;
    float ir = length(iq), ang = atan(iq.y, iq.x);
    float iris = smoothstep(0.52, 0.48, ir);
    vec3 irisCol = pal(seed + 0.08 * sin(ang * 22.0 + ir * 30.0) + uHue) * (0.5 + 0.8 * noise(vec2(ang * 4.0, ir * 9.0)));
    float pupil = smoothstep(0.08, 0.04, length(vec2(iq.x * (2.6 - 1.6 * uBeat), iq.y * 0.45)));
    vec3 c = mix(vec3(0.92, 0.9, 0.85), irisCol, iris);
    c = mix(c, vec3(0.0), pupil * iris);
    c += 0.6 * smoothstep(0.12, 0.0, length(iq - vec2(-0.16, 0.16))) * iris;  // a glint
    vec3 rim = pal(seed + 0.5) * 1.6;
    return vec4(mix(rim, c, inside), max(inside, lid));
}

// the Watcher: a mandala of eyes, rings turning opposite ways, all opening as it arrives
vec4 watcher(vec2 p)
{
    float r = length(p), an = atan(p.y, p.x);
    float rays = 0.5 + 0.5 * cos(an * 24.0 + sin(r * 10.0 - uTime) * 2.0);
    vec4 acc = vec4(pal(r * 0.8 + uHue + 0.5) * rays * 0.8, smoothstep(0.9, 0.2, r) * 0.65 * rays);  // see-through: the dome shows behind
    vec2 look = vec2(sin(uTime * 0.7), cos(uTime * 0.53)) * 0.6 * (1.0 - uBeat);
    for (int ring = 0; ring < 3; ring++) {
        int n = ring == 0 ? 1 : (ring == 1 ? 6 : 12);
        float R = ring == 0 ? 0.0 : (ring == 1 ? 0.3 : 0.58);
        float s = ring == 0 ? 0.17 : (ring == 1 ? 0.085 : 0.06);
        float turn = uTime * (ring == 1 ? 0.15 : -0.1);
        for (int i = 0; i < 12; i++) {
            if (i >= n) break;
            float phi = turn + float(i) * 2.0 * PI / float(n);
            vec2 c = R * vec2(cos(phi), sin(phi));
            vec2 q = p - c;
            if (length(q) > s * 1.2) continue;
            float seed = float(ring * 12 + i) * 0.137;
            float blink = step(0.985, sin(uTime * 1.3 + seed * 40.0));
            float open = clamp(uEntityAmt * 1.3 - float(ring) * 0.15, 0.0, 1.0) * (1.0 - blink);
            vec4 e = eye(q, s, open, look, seed);
            acc = mix(acc, vec4(e.rgb, 1.0), e.a);
        }
    }
    return acc;
}

// one jester: bell body in harlequin diamonds, a two-horned hat with bells, little eyes
vec4 jester(vec2 q, float s, float seed)
{
    q /= s;
    float body = smin(length(q - vec2(0.0, -0.35)) - 0.42, length(q - vec2(0.0, 0.05)) - 0.25, 0.15);
    body = smin(body, length(q - vec2(0.0, 0.42)) - 0.2, 0.08);
    vec2 hq = vec2(abs(q.x), q.y);
    float horn = min(sdSeg(hq, vec2(0.05, 0.56), vec2(0.25, 0.8)), sdSeg(hq, vec2(0.25, 0.8), vec2(0.44, 0.7))) - 0.06;
    float bell = length(hq - vec2(0.46, 0.64)) - 0.075;
    float eyeD = length(hq - vec2(0.08, 0.45)) - 0.05;
    vec2 dq = rot(PI / 4.0) * q * 6.0;
    float diamond = mod(floor(dq.x) + floor(dq.y), 2.0);
    vec3 c1 = pal(seed + uHue), c2 = pal(seed + uHue + 0.5);
    vec3 c = mix(c1, c2, diamond);
    c = mix(c, mix(c2, c1, step(0.5, fract(hq.x * 8.0))), step(horn, 0.0));
    c = mix(c, vec3(1.0, 0.85, 0.3), step(bell, 0.0));
    float pupil = length(hq - vec2(0.085, 0.44)) - 0.022;
    c = mix(c, vec3(1.0), step(eyeD, 0.0));
    c = mix(c, vec3(0.0), step(pupil, 0.0));
    float d = min(min(body, horn), bell);
    float edge = smoothstep(0.06, 0.0, abs(d)) * 1.5;
    return vec4(c + pal(seed + uHue + 0.25) * edge, smoothstep(0.03, -0.01, d));
}
vec4 jesters(vec2 p)
{
    vec4 acc = vec4(0.0);
    for (int i = 0; i < 5; i++) {
        float fi = float(i);
        float ph = uTime * 0.25 + fi * 2.0 * PI / 5.0;
        vec2 c = vec2(0.5 * cos(ph), 0.28 * sin(ph) - 0.05);
        c.y += abs(sin(uTime * 2.2 + fi)) * 0.12 + uBeat * 0.08;  // bouncing
        float s = 0.17 * (1.0 + 0.15 * sin(uTime * 3.0 + fi));
        vec2 q = p - c;
        q.x *= 1.0 + 0.3 * sin(uTime * 1.7 + fi) * (0.4 + uMid * 0.6);  // never quite the same shape twice
        q = rot(0.3 * sin(uTime * 1.5 + fi)) * q;
        if (length(q) > s * 1.3) continue;
        vec4 j = jester(q, s, fi * 0.21);
        acc = mix(acc, vec4(j.rgb, 1.0), j.a);
    }
    return acc;
}

// the Mantis: mirrored left/right. A triangular head with big faceted eyes, arms folded in prayer
// (spiked), a leaf-shaped body, thin legs, and fanned wings of light behind
float limb(vec2 p, vec2 a, vec2 b, vec2 c, float w) { return min(sdSeg(p, a, b), sdSeg(p, b, c)) - w; }
vec4 mantis(vec2 p)
{
    p = p * 1.15 + vec2(0.0, 0.05);
    vec2 m = vec2(abs(p.x), p.y);
    // wings
    vec2 w = m - vec2(0.0, -0.1);
    float wr = length(w), wa = atan(w.y, w.x);
    float fan = 0.5 + 0.5 * cos(wa * 34.0 + fbm(vec2(wr * 5.0, wa * 3.0) + uTime * 0.2) * 6.0);
    float wing = smoothstep(0.8, 0.45, wr) * smoothstep(-1.2, -0.3, wa) * smoothstep(1.3, 0.8, wa);
    vec4 acc = vec4(pal(wr + uHue + 0.6) * (0.2 + 0.9 * fan) * (0.6 + uHigh * 0.5), wing * 0.8);
    // legs (thin) and the body
    float legs = min(limb(m, vec2(0.03, -0.1), vec2(0.27, -0.02), vec2(0.36, -0.26), 0.008),
                     limb(m, vec2(0.04, -0.17), vec2(0.28, -0.34), vec2(0.32, -0.62), 0.008));
    vec2 ab = (m - vec2(0.0, -0.4)) / vec2(0.15, 0.33);
    float abdomen = (length(ab) - 1.0) * 0.15;
    float thorax = sdSeg(m, vec2(0.0, -0.08), vec2(0.0, 0.17)) - 0.035;
    vec2 hp = m - vec2(0.0, 0.17);  // the head: a triangle, chin down at y 0.17, eyes at its top corners
    float head = max(max(dot(hp, normalize(vec2(1.0, -0.6))) - 0.02, hp.y - 0.15), -hp.y - 0.01);
    // arms folded in prayer: thigh up and out, shin folding back down and in
    float arm = limb(m, vec2(0.02, 0.06), vec2(0.15, 0.2), vec2(0.06, -0.01), 0.022);
    float spikes = 1.0;
    for (int i = 0; i < 4; i++) {
        vec2 a = mix(vec2(0.15, 0.2), vec2(0.06, -0.01), 0.2 + 0.2 * float(i));
        spikes = min(spikes, sdSeg(m, a, a + vec2(0.035, -0.01)) - 0.004);
    }
    float antenna = sdSeg(m, vec2(0.04, 0.31), vec2(0.2 + 0.03 * sin(uTime * 2.0), 0.62)) - 0.004;
    float bodyD = min(min(min(abdomen, thorax), min(head, arm)), min(min(antenna, spikes), legs));
    float segs = 0.5 + 0.5 * sin(ab.y * 9.0);
    vec3 bodyCol = mix(vec3(0.12, 0.45, 0.22), vec3(0.45, 0.9, 0.5), segs * step(abdomen, 0.0) + 0.5 * step(0.0, abdomen));
    bodyCol *= 0.6 + 0.6 * smoothstep(0.0, -0.05, bodyD) + 0.3 * uMid;  // lighter toward the middle of each part
    float edge = smoothstep(0.01, 0.0, abs(bodyD));
    acc = mix(acc, vec4(bodyCol, 1.0), smoothstep(0.004, -0.004, bodyD));
    acc.rgb += pal(uHue + 0.3) * edge * 1.2;
    acc.a = max(acc.a, edge);
    // eyes: big, faceted, bulging from the head's top corners, glowing on the beat
    vec2 ep = (m - vec2(0.12, 0.31)) / vec2(0.06, 0.05);
    if (length(ep) < 1.0) {
        vec2 hx = ep * 5.0;
        float facet = smoothstep(0.3, 0.5, length(fract(hx + vec2(0.5 * mod(floor(hx.y), 2.0), 0.0)) - 0.5));
        vec3 ec = mix(pal(uHue + 0.15) * 1.4, vec3(0.03), facet * 0.6) * (0.7 + uBeat);
        ec += 0.5 * smoothstep(0.35, 0.0, length(ep - vec2(-0.3, 0.35)));
        acc = vec4(ec, 1.0);
    }
    return acc;
}

vec4 entity(vec2 p)
{
    if (uEntity == 0) return watcher(p);
    if (uEntity == 1) return jesters(p);
    return mantis(p);
}

void main()
{
    vec2 uv = gl_FragCoord.xy / uRes;
    vec2 p = (gl_FragCoord.xy - 0.5 * uRes) / uRes.y;
    p *= 1.0 - 0.06 * uBass;  // breathing with the bass
    vec3 col = vec3(0.0);
    float w0 = clamp(1.0 - abs(uStage - 0.0), 0.0, 1.0), w1 = clamp(1.0 - abs(uStage - 1.0), 0.0, 1.0);
    float w2 = clamp(1.0 - abs(uStage - 2.0), 0.0, 1.0), w3 = clamp(1.0 - abs(uStage - 3.0), 0.0, 1.0);
    if (w0 > 0.0) col += w0 * comeup(p);
    if (w1 > 0.0) col += w1 * chrysanthemum(p);
    if (w2 > 0.0) col += w2 * tunnel(p);
    if (w3 > 0.0) col += w3 * dome(p);
    // the repeating pattern laid over it all (screen blend), fading from one to the next
    if (uPatAmt > 0.0) {
        vec3 pat = pattern(p, uPatA);
        if (uPatT > 0.0) pat = mix(pat, pattern(p, uPatB), uPatT);
        pat = clamp(pat * uPatAmt, 0.0, 1.0);
        col = 1.0 - (1.0 - clamp(col, 0.0, 1.0)) * (1.0 - pat);
    }
    // the entity materialises out of the pattern (noisy at first) and dissolves back into it
    if (uEntityAmt > 0.01) {
        vec4 e = entity(p);
        float grow = smoothstep(1.0 - uEntityAmt, 1.0 - uEntityAmt + 0.25, fbm(p * 6.0 + uTime * 0.3) * 0.5 + uEntityAmt * 0.6);
        col = mix(col, e.rgb, e.a * grow);
    }
    // just dropped: the picture itself, breathing, before the trip takes it over
    if (uIntro > 0.0) {
        vec2 iq = p * (1.0 - 0.02 * sin(uTime * 0.8)) + 0.006 * vec2(sin(p.y * 9.0 + uTime), cos(p.x * 7.0 - uTime)) * (1.0 - uIntro);
        vec2 iuv = iq / vec2(uRes.x / uRes.y, 1.0) + 0.5;
        vec3 pic = M(iuv);
        pic = max(mix(vec3(dot(pic, vec3(0.299, 0.587, 0.114))), pic, 1.6), 0.0) * 0.72;  // a little deeper and richer, so pale things don't glow white
        col = mix(col, pic * (1.0 + 0.3 * uBass), uIntro * uMediaAmt * 0.92 * MA(iuv));
    }
    // the last frame, pulled inward and turned, its colours drifting: everything melts and trails
    vec2 fq = (uv - 0.5) * vec2(uRes.x / uRes.y, 1.0);
    fq = rot(0.004 + 0.012 * uMid) * fq * (0.985 - 0.012 * uBass);
    vec3 prev = texture(uPrev, fq / vec2(uRes.x / uRes.y, 1.0) + 0.5).rgb;
    if (any(isnan(prev)) || any(isinf(prev))) prev = vec3(0.0);  // one bad pixel must never spread through the trails
    prev = max(hueShift(prev, 0.01 + 0.02 * uHigh), 0.0);
    float fb = (0.5 + 0.15 * uStage / 3.0) * (1.0 - 0.8 * uIntro);
    col = mix(col, max(col, prev * 0.94), fb);
    if (any(isnan(col)) || any(isinf(col))) col = vec3(0.0);
    frag = vec4(min(col, vec3(64.0)), 1.0);
}
)";

// the wireframes: a strip, brightest down its middle, added on top of the picture
static const char* const kWireVertex = R"(#version 330 core
layout(location = 0) in vec2 pos;
layout(location = 1) in float across;
layout(location = 2) in vec3 col;
out float vAcross;
out vec3 vCol;
void main()
{
    vAcross = across;
    vCol = col;
    gl_Position = vec4(pos, 0.0, 1.0);
}
)";
static const char* const kWireFragment = R"(#version 330 core
in float vAcross;
in vec3 vCol;
out vec4 frag;
void main()
{
    float a = vAcross * vAcross;
    frag = vec4(vCol * (exp(-a * 16.0) * 1.5 + exp(-a * 2.5) * 0.35), 1.0);
}
)";

// FX: mirroring, chrome edge tracing, contrast
static const char* const kFx = R"(#version 330 core
out vec4 frag;
uniform sampler2D uScene;
uniform vec2 uRes;
uniform int uMirror;
uniform float uTrace, uContrast, uHue, uMood, uTurn;
uniform vec3 uSeedPal[3];  // a dropped picture's three main colours...
uniform float uSeedMix;     // ...and how much they colour everything
#define PI 3.14159265
// a dropped picture's colours, going round
vec3 seedRamp(float t)
{
    t = fract(t) * 3.0;
    vec3 c = t < 1.0 ? mix(uSeedPal[0], uSeedPal[1], t) : (t < 2.0 ? mix(uSeedPal[1], uSeedPal[2], t - 1.0) : mix(uSeedPal[2], uSeedPal[0], t - 2.0));
    return c / max(max(c.r, c.g), max(c.b, 0.08)) * 0.95;  // as bright as it'll go
}
vec3 pal(float t)
{
    vec3 bright = 0.5 + 0.5 * cos(6.2831 * (t + vec3(0.0, 0.33, 0.67)));
    bright = mix(bright, mix(vec3(1.0, 0.2, 0.65), vec3(0.2, 1.0, 0.5), 0.5 + 0.5 * cos(6.2831 * t)), 0.4);  // pulled toward pink and green
    vec3 dark = vec3(0.32, 0.18, 0.42) + vec3(0.38, 0.32, 0.45) * cos(6.2831 * (t + vec3(0.62, 0.42, 0.18)));
    vec3 c = mix(dark, bright, uMood);
    return uSeedMix > 0.0 ? mix(c, seedRamp(t) * mix(0.6, 1.0, uMood), 0.65 * uSeedMix) : c;
}
// 0 none, 1 left/right, 2 four ways, 3/4/5 kaleidoscopes of 6, 8 and 12
vec2 fold(vec2 p)
{
    if (uMirror == 1) return vec2(abs(p.x), p.y);
    if (uMirror == 2) return abs(p);
    if (uMirror >= 3) {
        float n = uMirror == 3 ? 6.0 : (uMirror == 4 ? 8.0 : 12.0);
        float seg = 2.0 * PI / n, r = length(p), a = atan(p.y, p.x) + uTurn;
        a = abs(mod(a, seg) - seg * 0.5);
        return r * vec2(cos(a), sin(a));
    }
    return p;
}
float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }
void main()
{
    float asp = uRes.x / uRes.y;
    vec2 p = (gl_FragCoord.xy / uRes - 0.5) * vec2(asp, 1.0);
    vec2 uv = fold(p) / vec2(asp, 1.0) + 0.5;
    vec3 c = texture(uScene, uv).rgb;
    // trace the edges: how fast the brightness changes here (a Sobel filter), drawn as neon
    vec2 px = 1.5 / uRes;
    float tl = luma(texture(uScene, uv + px * vec2(-1, 1)).rgb), t = luma(texture(uScene, uv + px * vec2(0, 1)).rgb);
    float tr = luma(texture(uScene, uv + px * vec2(1, 1)).rgb), l = luma(texture(uScene, uv + px * vec2(-1, 0)).rgb);
    float r = luma(texture(uScene, uv + px * vec2(1, 0)).rgb), bl = luma(texture(uScene, uv + px * vec2(-1, -1)).rgb);
    float b = luma(texture(uScene, uv + px * vec2(0, -1)).rgb), br = luma(texture(uScene, uv + px * vec2(1, -1)).rgb);
    float gx = (tr + 2.0 * r + br) - (tl + 2.0 * l + bl), gy = (tl + 2.0 * t + tr) - (bl + 2.0 * b + br);
    float e = sqrt(gx * gx + gy * gy);
    // ...in chrome: bands of light and dark reflected along each edge, tinted
    float band = 0.5 + 0.5 * sin(atan(gy, gx) * 2.0 + luma(c) * 8.0);
    vec3 chrome = mix(vec3(0.15, 0.1, 0.2), vec3(1.0), smoothstep(0.3, 0.7, band)) * mix(vec3(1.0), pal(uHue + luma(c) * 0.8 + 0.5), 0.45);
    c += chrome * smoothstep(0.08, 0.6, e) * uTrace * 1.2;
    // contrast: an S-curve, and a little more colour
    vec3 s = clamp(c, 0.0, 1.0);
    c = mix(c, s * s * (3.0 - 2.0 * s) + max(c - 1.0, 0.0), uContrast);
    c = max(mix(vec3(luma(c)), c, 1.0 + 0.4 * uContrast), 0.0);
    if (any(isnan(c)) || any(isinf(c))) c = vec3(0.0);
    frag = vec4(c, 1.0);
}
)";

// BRIGHT: only what's bright enough to glow
static const char* const kBright = R"(#version 330 core
out vec4 frag;
uniform sampler2D uSrc;
uniform vec2 uRes;
void main()
{
    vec3 c = texture(uSrc, gl_FragCoord.xy / uRes).rgb;
    frag = vec4(max(c - 0.65, 0.0) * 1.5, 1.0);
}
)";
// BLUR: a soft smear along one direction (run across, then down)
static const char* const kBlur = R"(#version 330 core
out vec4 frag;
uniform sampler2D uSrc;
uniform vec2 uRes, uDir;
void main()
{
    vec2 uv = gl_FragCoord.xy / uRes;
    const float w[5] = float[](0.227, 0.195, 0.122, 0.054, 0.016);
    vec3 c = texture(uSrc, uv).rgb * w[0];
    for (int i = 1; i < 5; i++) {
        c += texture(uSrc, uv + uDir * float(i) * 1.6).rgb * w[i];
        c += texture(uSrc, uv - uDir * float(i) * 1.6).rgb * w[i];
    }
    if (any(isnan(c)) || any(isinf(c))) c = vec3(0.0);
    frag = vec4(c, 1.0);
}
)";

static const char* const kPresent = R"(#version 330 core
out vec4 frag;
uniform sampler2D uScene, uBloom;
uniform vec2 uRes;
uniform float uBass, uTime, uBloomAmt;
float hash(vec2 p) { p = fract(p * vec2(123.34, 456.21)); p += dot(p, p + 45.32); return fract(p.x * p.y); }
void main()
{
    vec2 uv = gl_FragCoord.xy / uRes, d = uv - 0.5;
    float k = 0.003 + 0.008 * uBass;  // colours split apart a little on the bass
    vec3 c = vec3(texture(uScene, 0.5 + d * (1.0 + k)).r, texture(uScene, uv).g, texture(uScene, 0.5 + d * (1.0 - k)).b);
    c += texture(uBloom, uv).rgb * uBloomAmt;
    c *= 1.0 - 0.7 * dot(d, d);
    c = c / (1.0 + 0.12 * c);
    c += (hash(gl_FragCoord.xy + fract(uTime) * 100.0) - 0.5) * 0.025;
    if (any(isnan(c)) || any(isinf(c))) c = vec3(0.0);
    frag = vec4(c, 1.0);
}
)";
