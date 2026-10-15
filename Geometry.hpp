// Shapes from 2D to 4D (copied from Hyperroom): corners, walls, faces and edges.
// hyperspace uses the edges for its wireframes: tesseracts, 24-cells, icosahedra...
#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <vector>

namespace hr {

struct V4 {
    float x = 0, y = 0, z = 0, w = 0;
    float& operator[](int i) { return (&x)[i]; }
    float operator[](int i) const { return (&x)[i]; }
};
inline V4 operator+(V4 a, V4 b) { return {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w}; }
inline V4 operator-(V4 a, V4 b) { return {a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w}; }
inline V4 operator*(V4 a, float s) { return {a.x * s, a.y * s, a.z * s, a.w * s}; }
inline float dot(V4 a, V4 b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }
inline float len(V4 a) { return std::sqrt(dot(a, a)); }
inline V4 lerp(V4 a, V4 b, float t) { return a + (b - a) * t; }

struct Shape {
    const char* name;
    const char* shortName;
    int dim;                               // 2 plate, 3 room, 4 hyper-room
    std::vector<V4> verts;                 // furthest corner 1 from the middle
    std::vector<V4> normals;               // walls: pointing out...
    std::vector<float> dists;              // ...and how far from the middle
    std::vector<std::vector<int>> faces;   // corners of each face, in order round it
    std::vector<std::vector<int>> faceWalls;  // which walls each face is part of
    std::vector<std::array<int, 2>> edges;
    std::vector<std::vector<int>> edgeWalls;
    // the sound's character, worked out once from the shape (see finish())
    float roundness = 0;                   // how close the nearest wall is (1: a ball, low: pointy)
    std::vector<std::array<int, 2>> pairs; // second bounces: off wall [0], then wall [1]
    std::vector<float> pairGain;           // facing (parallel) pairs ring loudest: flutter echo
    float ray[16] = {};                    // the tail: how far each of 16 sound rays goes in two bounces
    float flutter = 0, flutterWidth = 2;   // walls facing each other: how much they ping back and forth, and how far apart
    int walls() const { return (int)normals.size(); }
};

// how far from the middle to the shape's skin in direction `dir` (only the
// shape's own dimensions count; a direction with none of them gives 0)
inline float radial(const Shape& s, V4 dir)
{
    for (int i = s.dim; i < 4; ++i) dir[i] = 0;
    const float l = len(dir);
    if (l < 1e-6f) return 0;
    dir = dir * (1 / l);
    float r = 1e9f;
    for (size_t f = 0; f < s.normals.size(); ++f) {
        const float c = dot(s.normals[f], dir);
        if (c > 1e-6f) r = std::min(r, s.dists[f] / c);
    }
    return r > 1e8f ? 1.0f : r;
}
// where a point of another shape's skin lands on this one's (for morphing)
inline V4 onto(const Shape& s, V4 p)
{
    for (int i = s.dim; i < 4; ++i) p[i] = 0;
    const float l = len(p);
    return l < 1e-6f ? V4{} : p * (radial(s, p) / l);
}

namespace detail {

inline float det3(float a0, float a1, float a2, float b0, float b1, float b2, float c0, float c1, float c2)
{
    return a0 * (b1 * c2 - b2 * c1) - a1 * (b0 * c2 - b2 * c0) + a2 * (b0 * c1 - b1 * c0);
}

// a direction square to all of `d` (dim-1 of them), in `dim` dimensions
inline V4 perpendicular(int dim, const V4* d)
{
    if (dim == 2) return {-d[0].y, d[0].x, 0, 0};
    if (dim == 3) return {d[0].y * d[1].z - d[0].z * d[1].y, d[0].z * d[1].x - d[0].x * d[1].z, d[0].x * d[1].y - d[0].y * d[1].x, 0};
    const V4 &a = d[0], &b = d[1], &c = d[2];
    return {det3(a.y, a.z, a.w, b.y, b.z, b.w, c.y, c.z, c.w), -det3(a.x, a.z, a.w, b.x, b.z, b.w, c.x, c.z, c.w),
            det3(a.x, a.y, a.w, b.x, b.y, b.w, c.x, c.y, c.w), -det3(a.x, a.y, a.z, b.x, b.y, b.z, c.x, c.y, c.z)};
}

// the walls: every set of `dim` corners whose flat goes along the outside
inline void findWalls(Shape& s)
{
    const int n = (int)s.verts.size(), k = s.dim;
    std::vector<int> idx(k);
    for (int i = 0; i < k; ++i) idx[i] = i;
    while (true) {
        V4 d[3];
        for (int i = 1; i < k; ++i) d[i - 1] = s.verts[idx[i]] - s.verts[idx[0]];
        V4 nrm = perpendicular(k, d);
        const float l = len(nrm);
        if (l > 1e-5f) {
            nrm = nrm * (1 / l);
            float dist = dot(nrm, s.verts[idx[0]]);
            if (dist < 0) { nrm = nrm * -1; dist = -dist; }
            bool outside = dist > 1e-4f;
            for (int v = 0; v < n && outside; ++v)
                if (dot(nrm, s.verts[v]) > dist + 1e-4f) outside = false;
            for (const V4& m : s.normals)
                if (outside && dot(m, nrm) > 0.9999f) outside = false;
            if (outside) {
                s.normals.push_back(nrm);
                s.dists.push_back(dist);
            }
        }
        // next set of k corners
        int i = k - 1;
        while (i >= 0 && idx[i] == n - k + i) --i;
        if (i < 0) break;
        ++idx[i];
        for (int j = i + 1; j < k; ++j) idx[j] = idx[j - 1] + 1;
    }
}

inline bool onWall(const Shape& s, int wall, int v) { return std::fabs(dot(s.normals[wall], s.verts[v]) - s.dists[wall]) < 1e-4f; }

// put a face's corners in order round it
inline void sortRound(const Shape& s, std::vector<int>& f)
{
    V4 c;
    for (int v : f) c = c + s.verts[v];
    c = c * (1.0f / f.size());
    V4 e1 = s.verts[f[0]] - c;
    e1 = e1 * (1 / len(e1));
    V4 e2;
    float best = 0;
    for (int v : f) {
        V4 d = s.verts[v] - c;
        d = d - e1 * dot(d, e1);
        if (len(d) > best) { best = len(d); e2 = d * (1 / best); }
    }
    std::sort(f.begin(), f.end(), [&](int a, int b) {
        const V4 da = s.verts[a] - c, db = s.verts[b] - c;
        return std::atan2(dot(da, e2), dot(da, e1)) < std::atan2(dot(db, e2), dot(db, e1));
    });
}

} // namespace detail

// the 16 directions sound rays leave in, for each kind of shape (2D, 3D, 4D)
inline const V4* rayDirs(int dim)
{
    struct Dirs {
        V4 d[3][16];
        Dirs()
        {
            for (int i = 0; i < 16; ++i) {
                const float a = 6.2831853f * (i + 0.5f) / 16;
                d[0][i] = {std::cos(a), std::sin(a), 0, 0};
                const float y = 1 - 2 * (i + 0.5f) / 16, r = std::sqrt(1 - y * y), ph = i * 2.3999632f;
                d[1][i] = {r * std::cos(ph), y, r * std::sin(ph), 0};
                uint32_t sd = 0x9E3779B9u * (i + 1);
                V4 g;
                for (int k = 0; k < 4; ++k) {
                    sd = sd * 1664525u + 1013904223u;
                    g[k] = (sd >> 8) / 8388608.0f - 1;
                }
                d[2][i] = g * (1 / len(g));
            }
        }
    };
    static const Dirs dirs;  // made once, safely, the first time
    return dirs.d[std::clamp(dim, 2, 4) - 2];
}

// where the sound starts inside the room (you listen from the middle, or where you put the ring)
static const V4 kSource = {0.22f, -0.12f, 0.16f, 0.1f};

namespace detail {

// a sound ray: from `p` along `d`, bouncing off the walls `bounces` times; how far it went
inline float traceRay(const Shape& s, V4 p, V4 d, int bounces)
{
    float total = 0;
    for (int b = 0; b < bounces; ++b) {
        float best = 1e9f;
        int hit = -1;
        for (int f = 0; f < s.walls(); ++f) {
            const float c = dot(s.normals[f], d);
            if (c < 1e-5f) continue;
            const float t = (s.dists[f] - dot(s.normals[f], p)) / c;
            if (t > 1e-5f && t < best) { best = t; hit = f; }
        }
        if (hit < 0 || best > 2.01f) break;  // slipped out through a corner: stop there
        p = p + d * best;
        total += best;
        d = d - s.normals[hit] * (2 * dot(s.normals[hit], d));
    }
    return total;
}

inline void character(Shape& s)
{
    const int W = s.walls();
    s.roundness = *std::min_element(s.dists.begin(), s.dists.end());
    // rays start where the sound does (only the shape's own dimensions)
    V4 src = kSource;
    for (int i = s.dim; i < 4; ++i) src[i] = 0;
    for (int i = 0; i < 16; ++i) s.ray[i] = std::clamp(traceRay(s, src, rayDirs(s.dim)[i], 2), 0.3f, 4.0f);
    // flutter: for each wall, how squarely the one opposite faces it (only near-parallel counts)
    float sum = 0, wsum = 0;
    for (int i = 0; i < W; ++i) {
        int far = 0;
        for (int j = 0; j < W; ++j)
            if (dot(s.normals[i], s.normals[j]) < dot(s.normals[i], s.normals[far])) far = j;
        const float facing = std::pow(std::max(0.0f, -dot(s.normals[i], s.normals[far])), 8.0f);
        sum += facing;
        wsum += facing * (s.dists[i] + s.dists[far]);
    }
    s.flutter = W ? sum / W : 0;
    s.flutterWidth = sum > 0 ? wsum / sum : 2;
    // second bounces: each wall with its 3 nearest neighbours (corners) and the one facing it.
    // The sphere has so many walls it just gets the first bounces.
    if (W > 24) return;
    for (int i = 0; i < W; ++i) {
        std::vector<int> o;
        for (int j = 0; j < W; ++j)
            if (j != i) o.push_back(j);
        std::sort(o.begin(), o.end(), [&](int a, int b) { return dot(s.normals[i], s.normals[a]) > dot(s.normals[i], s.normals[b]); });
        for (int k = 0; k < 3 && k < (int)o.size(); ++k) {
            s.pairs.push_back({i, o[k]});
            s.pairGain.push_back(0.55f);
        }
        const int far = o.back();
        const float facing = std::max(0.0f, -dot(s.normals[i], s.normals[far]));
        s.pairs.push_back({i, far});
        s.pairGain.push_back(0.25f + 1.1f * std::pow(facing, 4.0f));  // truly parallel walls: strong flutter
    }
}

inline void finish(Shape& s)
{
    // furthest corner at 1
    float r = 0;
    for (const V4& v : s.verts) r = std::max(r, len(v));
    for (V4& v : s.verts) v = v * (1 / r);
    findWalls(s);
    const int n = (int)s.verts.size(), W = s.walls();
    // faces: a plate is one face; a room's walls are its faces; in 4D, where two walls meet
    if (s.dim == 2) {
        std::vector<int> f;
        for (int v = 0; v < n; ++v) f.push_back(v);
        s.faces.push_back(f);
        s.faceWalls.push_back({});
    } else {
        for (int a = 0; a < W; ++a)
            for (int b = (s.dim == 3 ? a : a + 1); b < (s.dim == 3 ? a + 1 : W); ++b) {
                std::vector<int> f;
                for (int v = 0; v < n; ++v)
                    if (onWall(s, a, v) && onWall(s, b, v)) f.push_back(v);
                if (f.size() < 3) continue;
                sortRound(s, f);
                s.faces.push_back(f);
                s.faceWalls.push_back(s.dim == 3 ? std::vector<int>{a} : std::vector<int>{a, b});
            }
    }
    // edges: round each face
    for (const auto& f : s.faces)
        for (size_t i = 0; i < f.size(); ++i) {
            std::array<int, 2> e = {std::min(f[i], f[(i + 1) % f.size()]), std::max(f[i], f[(i + 1) % f.size()])};
            if (std::find(s.edges.begin(), s.edges.end(), e) == s.edges.end()) s.edges.push_back(e);
        }
    for (const auto& e : s.edges) {
        std::vector<int> w;
        for (int a = 0; a < W; ++a)
            if (onWall(s, a, e[0]) && onWall(s, a, e[1])) w.push_back(a);
        s.edgeWalls.push_back(w);
    }
    character(s);
}

inline Shape polygon(const char* name, const char* sh, int n)
{
    Shape s;
    s.name = name;
    s.shortName = sh;
    s.dim = 2;
    for (int i = 0; i < n; ++i) {
        const float a = 1.5707963f + 6.2831853f * i / n;
        s.verts.push_back({std::cos(a), std::sin(a), 0, 0});
    }
    finish(s);
    return s;
}

inline Shape solid(const char* name, const char* sh, int dim, std::vector<V4> v)
{
    Shape s;
    s.name = name;
    s.shortName = sh;
    s.dim = dim;
    s.verts = std::move(v);
    finish(s);
    return s;
}

// all the ways of placing values in 3 cyclic positions with every sign
inline void cyclicSigns(std::vector<V4>& out, float a, float b, float c)
{
    const float v[3] = {a, b, c};
    for (int rot = 0; rot < 3; ++rot)
        for (int sg = 0; sg < 8; ++sg) {
            V4 p;
            for (int i = 0; i < 3; ++i) p[(i + rot) % 3] = v[i] * ((sg >> i) & 1 ? -1 : 1);
            bool dup = false;
            for (const V4& q : out) dup |= len(q - p) < 1e-5f;
            if (!dup) out.push_back(p);
        }
}

inline std::vector<V4> icosahedron()
{
    std::vector<V4> v;
    cyclicSigns(v, 0, 1, 1.6180340f);
    return v;
}

// a sphere: an icosahedron with every triangle split in four, pushed out round
inline std::vector<V4> geodesic()
{
    std::vector<V4> v = icosahedron();
    const size_t n = v.size();
    float edge = 1e9f;
    for (size_t i = 0; i < n; ++i)
        for (size_t j = i + 1; j < n; ++j) edge = std::min(edge, len(v[i] - v[j]));
    for (size_t i = 0; i < n; ++i)
        for (size_t j = i + 1; j < n; ++j)
            if (len(v[i] - v[j]) < edge * 1.01f) v.push_back((v[i] + v[j]) * 0.5f);
    for (V4& p : v) p = p * (1 / len(p));
    return v;
}

} // namespace detail

// the modes, and which shapes each one's SIDES knob goes through
enum Mode { kPlate = 0, kRoom, kHyper, kNumModes };
static const char* const kModeNames[kNumModes] = {"2D PLATE", "3D ROOM", "4D ROOM"};
static constexpr int kModeFirst[kNumModes + 1] = {0, 9, 16, 20};
static constexpr int kNumShapes = 20;

class Library {
public:
    Library()
    {
        using namespace detail;
        static const char* const pn[9][2] = {{"TRIANGLE", "TRI"}, {"SQUARE", "SQR"}, {"PENTAGON", "PENT"}, {"HEXAGON", "HEX"},
                                             {"HEPTAGON", "HEPT"}, {"OCTAGON", "OCT"}, {"DECAGON", "DEC"}, {"DODECAGON", "DODG"}, {"CIRCLE", "CIRC"}};
        static const int sides[9] = {3, 4, 5, 6, 7, 8, 10, 12, 24};
        for (int i = 0; i < 9; ++i)
            fShapes.push_back(polygon(pn[i][0], pn[i][1], sides[i]));

        const float a = std::sqrt(2.0f);  // a pyramid with every edge the same, its middle a quarter of the way up
        const float h = a * 1.0f / 4;
        std::vector<V4> pyr = {{1, -h, 1, 0}, {1, -h, -1, 0}, {-1, -h, 1, 0}, {-1, -h, -1, 0}, {0, a - h, 0, 0}};
        std::vector<V4> cube, octa, dode;
        for (int i = 0; i < 8; ++i) cube.push_back({i & 1 ? 1.0f : -1.0f, i & 2 ? 1.0f : -1.0f, i & 4 ? 1.0f : -1.0f, 0});
        for (int i = 0; i < 6; ++i) { V4 p; p[i / 2] = i & 1 ? -1.0f : 1.0f; octa.push_back(p); }
        dode = cube;
        cyclicSigns(dode, 0, 0.6180340f, 1.6180340f);
        fShapes.push_back(solid("TETRAHEDRON", "TET", 3, {{1, 1, 1, 0}, {1, -1, -1, 0}, {-1, 1, -1, 0}, {-1, -1, 1, 0}}));
        fShapes.push_back(solid("PYRAMID", "PYR", 3, pyr));
        fShapes.push_back(solid("CUBE", "CUBE", 3, cube));
        fShapes.push_back(solid("OCTAHEDRON", "OCTA", 3, octa));
        fShapes.push_back(solid("DODECAHEDRON", "DODE", 3, dode));
        fShapes.push_back(solid("ICOSAHEDRON", "ICOS", 3, icosahedron()));
        fShapes.push_back(solid("SPHERE", "SPHR", 3, geodesic()));

        const float q = 1 / std::sqrt(5.0f);
        std::vector<V4> tess, c16, c24;
        for (int i = 0; i < 16; ++i)
            tess.push_back({i & 1 ? 1.0f : -1.0f, i & 2 ? 1.0f : -1.0f, i & 4 ? 1.0f : -1.0f, i & 8 ? 1.0f : -1.0f});
        for (int i = 0; i < 8; ++i) { V4 p; p[i / 2] = i & 1 ? -1.0f : 1.0f; c16.push_back(p); }
        for (int i = 0; i < 4; ++i)
            for (int j = i + 1; j < 4; ++j)
                for (int sg = 0; sg < 4; ++sg) { V4 p; p[i] = sg & 1 ? -1.0f : 1.0f; p[j] = sg & 2 ? -1.0f : 1.0f; c24.push_back(p); }
        fShapes.push_back(solid("5-CELL", "5CEL", 4, {{1, 1, 1, -q}, {1, -1, -1, -q}, {-1, 1, -1, -q}, {-1, -1, 1, -q}, {0, 0, 0, 4 * q}}));
        fShapes.push_back(solid("TESSERACT", "TESS", 4, tess));
        fShapes.push_back(solid("16-CELL", "16CL", 4, c16));
        fShapes.push_back(solid("24-CELL", "24CL", 4, c24));
    }
    const Shape& operator[](int i) const { return fShapes[std::clamp(i, 0, kNumShapes - 1)]; }
    static int modeOf(int shape)
    {
        int m = 0;
        while (m < kNumModes - 1 && shape >= kModeFirst[m + 1]) ++m;
        return m;
    }

private:
    std::vector<Shape> fShapes;
};

// built once, read by everything (sound and window)
inline const Library& library()
{
    static const Library lib;
    return lib;
}

// ---- turning the room
// It keeps turning at SPIN. How it turns depends on what kind of shape it is,
// mixed when morphing between kinds (so nothing jumps):
//  plate: spins in its own plane; room: turns round the up axis;
//  4D: also turns through the 4th dimension (the hypercube turning inside out).
// Each way of turning has its own angle, kept between -pi and pi.
struct Pose {
    float a2 = 0, a3 = 0, a4 = 0, a5 = 0;  // plate, room, the two 4D turns
    float k2 = 0, k3 = 1, k4 = 0;          // how much plate / room / 4D (adds to 1)

    void advance(float radians)
    {
        auto wrap = [](float& a, float d) { a = std::remainder(a + d, 6.2831853f); };
        wrap(a2, radians);
        wrap(a3, radians);
        wrap(a4, radians * 0.7f);
        wrap(a5, radians * 0.45f);
    }
    V4 apply(V4 p) const
    {
        auto rot = [](V4& v, int i, int j, float a) {
            const float c = std::cos(a), s = std::sin(a), x = v[i], y = v[j];
            v[i] = c * x - s * y;
            v[j] = s * x + c * y;
        };
        rot(p, 0, 1, a2 * k2);           // plate: in its plane
        rot(p, 0, 2, a3 * (k3 + k4));    // room: round the up axis
        rot(p, 0, 3, a4 * k4);           // 4D: through the 4th dimension
        rot(p, 1, 3, a5 * k4);
        rot(p, 1, 2, 0.32f * (k3 + k4)); // tipped a little toward you
        return p;
    }
};

// keep a point (where you listen) inside the turned shape, a little in from the walls
inline V4 inside(const Shape& s, const Pose& pose, V4 p)
{
    float k = 1;
    for (int f = 0; f < s.walls(); ++f) {
        const float c = dot(pose.apply(s.normals[f]), p);
        if (c > 1e-6f) k = std::min(k, 0.8f * s.dists[f] / c);
    }
    return p * k;
}

// what the SIDES knob or the playlist is pointing at right now: two shapes and how far between
struct Blend {
    int a = 11, b = 11;
    float t = 0;
    int posA = 0;  // a's place in the list (the sound keeps each shape in the bank for its place's odd/even)
    float dimWeight(int d) const
    {
        const Library& L = library();
        return (L[a].dim == d ? 1 - t : 0) + (L[b].dim == d ? t : 0);
    }
};

} // namespace hr
