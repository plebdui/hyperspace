// Wireframes: glowing lines drawn over the scene (and so caught in its melting
// trails). Each line becomes a thin strip whose brightness falls off from its
// middle, added on top of what's there, so crossings get brighter like light.
//   Metatron's cube  - 13 circles' centres, every one joined to every other (come-up, chrysanthemum)
//   a polytope       - a 3D or 4D shape turning, the 4th dimension shown as perspective
//   the ring tunnel  - polygon rings rushing toward you, joined corner to corner (breakthrough)
//   a ring of hypercubes around the entity (hyperspace)
#pragma once

#include "Geometry.hpp"
#include <cmath>
#include <vector>

struct Rgb { float r, g, b; };

// the scene shader's colours (same formula), so the lines match
inline Rgb palette(float t, float mood)
{
    auto c = [](float a, float b, float x) { return a + b * std::cos(6.2831f * x); };
    Rgb bright = {c(0.5f, 0.5f, t), c(0.5f, 0.5f, t + 0.33f), c(0.5f, 0.5f, t + 0.67f)};
    const float k = 0.5f + 0.5f * std::cos(6.2831f * t);  // pulled toward pink and green
    bright = {bright.r * 0.6f + 0.4f * (1.0f + (0.2f - 1.0f) * k), bright.g * 0.6f + 0.4f * (0.2f + 0.8f * k), bright.b * 0.6f + 0.4f * (0.65f - 0.15f * k)};
    Rgb dark = {c(0.32f, 0.38f, t + 0.62f), c(0.18f, 0.32f, t + 0.42f), c(0.42f, 0.45f, t + 0.18f)};
    return {dark.r + (bright.r - dark.r) * mood, dark.g + (bright.g - dark.g) * mood, dark.b + (bright.b - dark.b) * mood};
}

class Wires {
public:
    // per vertex: x, y (screen, -1..1), across (-1..1 over the strip's width), r, g, b
    std::vector<float> data;
    float aspect = 16.0f / 9;  // width / height

    void clear() { data.clear(); }

    // a line between two points in picture units (height -0.5..0.5, width by aspect)
    void line(float x0, float y0, float x1, float y1, float width, Rgb c, float a)
    {
        if (a < 0.003f) return;
        float dx = x1 - x0, dy = y1 - y0;
        const float l = std::sqrt(dx * dx + dy * dy);
        if (l < 1e-5f) return;
        const float nx = -dy / l * width, ny = dx / l * width;
        auto v = [&](float x, float y, float across) {
            data.insert(data.end(), {x * 2 / aspect, y * 2, across, c.r * a, c.g * a, c.b * a});
        };
        v(x0 + nx, y0 + ny, 1); v(x0 - nx, y0 - ny, -1); v(x1 + nx, y1 + ny, 1);
        v(x1 + nx, y1 + ny, 1); v(x0 - nx, y0 - ny, -1); v(x1 - nx, y1 - ny, -1);
    }

    // a shape from the library, turned in 4D by angles a[0..5] (planes xy xz xw yz yw zw)
    void polytope(int shape, float cx, float cy, float size, const float* a, Rgb c, float alpha, float width = 0.004f)
    {
        const hr::Shape& s = hr::library()[shape];
        std::vector<float> px(s.verts.size()), py(s.verts.size()), depth(s.verts.size());
        for (size_t i = 0; i < s.verts.size(); ++i) {
            hr::V4 v = s.verts[i];
            static const int planes[6][2] = {{0, 1}, {0, 2}, {0, 3}, {1, 2}, {1, 3}, {2, 3}};
            for (int k = 0; k < 6; ++k) {
                const float cs = std::cos(a[k]), sn = std::sin(a[k]);
                const float x = v[planes[k][0]], y = v[planes[k][1]];
                v[planes[k][0]] = cs * x - sn * y;
                v[planes[k][1]] = sn * x + cs * y;
            }
            const float s4 = 2.2f / (2.2f - v.w);           // the 4th dimension as perspective
            const float s3 = 3.5f / (3.5f - v.z * s4);       // then the usual kind
            px[i] = cx + v.x * s4 * s3 * size;
            py[i] = cy + v.y * s4 * s3 * size;
            depth[i] = (v.z + v.w) * 0.5f;
        }
        for (const auto& e : s.edges) {
            const float near = 0.55f + 0.45f * std::max(-1.0f, std::min(1.0f, depth[e[0]] + depth[e[1]]));
            line(px[e[0]], py[e[0]], px[e[1]], py[e[1]], width, c, alpha * near);
        }
    }

    // Metatron's cube: a centre, an inner ring of 6 and an outer ring of 6, all joined
    void metatron(float cx, float cy, float size, float turn, Rgb c, float alpha)
    {
        float x[13], y[13];
        x[0] = cx;
        y[0] = cy;
        for (int i = 0; i < 6; ++i) {
            const float an = turn + i * 1.0471976f;
            x[1 + i] = cx + std::cos(an) * size;
            y[1 + i] = cy + std::sin(an) * size;
            x[7 + i] = cx + std::cos(an) * size * 2;
            y[7 + i] = cy + std::sin(an) * size * 2;
        }
        for (int i = 0; i < 13; ++i)
            for (int j = i + 1; j < 13; ++j) line(x[i], y[i], x[j], y[j], 0.0025f, c, alpha * 0.6f);
    }

    // rings of `sides` corners rushing toward you; `travel` keeps growing. Ring k sits at depth
    // (k + 1 - frac(travel)) / rings, so when one passes you the next takes its place seamlessly.
    void tunnel(int sides, float travel, float twist, float hue, float mood, float alpha)
    {
        const int rings = 16;
        const float slide = travel - std::floor(travel);
        std::vector<float> px, py;
        for (int k = 0; k < rings; ++k) {
            const float z = (k + 1 - slide) / rings;  // near (small) .. far (1)
            const float r = 0.07f / z;
            const float fade = alpha * std::min(1.0f, (1 - z) * 2.5f) * std::min(1.0f, z * rings * 0.8f);
            const Rgb c = palette(hue + z * 0.6f, mood);
            std::vector<float> nx(sides), ny(sides);
            for (int i = 0; i < sides; ++i) {
                const float an = twist + z * 2.5f + i * 6.2831853f / sides;
                nx[i] = std::cos(an) * r;
                ny[i] = std::sin(an) * r;
            }
            for (int i = 0; i < sides; ++i) {
                line(nx[i], ny[i], nx[(i + 1) % sides], ny[(i + 1) % sides], 0.003f + 0.004f * std::min(r, 1.0f), c, fade);
                if (k > 0) line(nx[i], ny[i], px[i], py[i], 0.002f + 0.002f * std::min(r, 1.0f), c, fade * 0.5f);
            }
            px = nx;
            py = ny;
        }
    }
};
