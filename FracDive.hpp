// The fractal dive (dimension 6 / the 2D fractals): zooms in at a fixed rate, always steering
// toward the most detailed spot on screen.
//
// Every 0.3 s it samples a coarse grid over what's visible (in double precision, on the CPU),
// scores each point by how much the escape counts change around it (the filigree on the edge of
// the set scores high, flat black inside and smooth outer bands score low), and eases the centre
// toward the winner. The GPU draws in single precision, which runs out of detail around
// 1/10000th of the start view, so at that depth the dive fades out and starts again from the top.
#pragma once
#include <algorithm>
#include <cmath>

struct FracDive {
    static constexpr double kRate = 0.25;     // zoom speed: the view shrinks by e^0.25 (x1.28) every second
    double speed = 1;                         // settings: times kRate
    bool twin = true;                         // match the fitted mirrored view in the shader
    bool seek = true;                         // settings: steer toward the detail (off = straight down the middle)
    static constexpr double kMinScale = 1.5e-4;
    int kind = -1;                            // which fractal (0 Mandelbrot, 1 Julia, 2 Burning Ship, 3 Tricorn, 4 Multibrot)
    double cx = 0, cy = 0, scale = 1.8, home = 1.8;  // centre and view size (the strip is scale*0.5 tall)
    double tx = 0, ty = 0;                    // the detail target the centre eases toward
    double jx = -0.7269, jy = 0.1889;         // Julia's c (wobbles with the music, only while shallow)
    float searchIn = 0, fade = 0;             // seconds to the next search; fade 0..1 (the dive's own alpha)
    bool leaving = false;                     // fading out to restart
    float base = 0;                           // lowest escape count on screen: the colouring counts from here
    int iters() const { return std::min(460, 160 + (int)(55.0 * std::log(home / scale))); }

    void restart()
    {
        static const double hx[5] = {-0.6, 0.0, -0.45, -0.25, 0.0}, hy[5] = {0.0, 0.0, -0.5, 0.0, 0.0};
        static const double hs[5] = {1.8, 1.9, 1.9, 2.0, 1.7};
        cx = tx = hx[kind]; cy = ty = hy[kind]; scale = home = hs[kind];
        searchIn = 0; leaving = false;
    }

    // smooth escape count at c (or z0 for Julia); -1 = never escaped (inside the set)
    double escape(double x, double y, int maxIt) const
    {
        double zx = 0, zy = 0, ccx = x, ccy = y;
        if (kind == 1) { zx = x; zy = y; ccx = jx; ccy = jy; }
        for (int k = 0; k < maxIt; ++k) {
            if (kind == 2) { zx = std::fabs(zx); zy = std::fabs(zy); }
            if (kind == 3) zy = -zy;
            double nx = zx * zx - zy * zy, ny = 2 * zx * zy;
            if (kind == 4) { double t = nx * zx - ny * zy; ny = nx * zy + ny * zx; nx = t; }
            zx = nx + ccx; zy = ny + ccy;
            const double m2 = zx * zx + zy * zy;
            if (m2 > 256) return k - std::log2(std::log2(m2)) + 4;
        }
        return -1;
    }

    // look over the visible strip and pick the busiest point (near the centre wins ties, so it doesn't lurch)
    void search(double asp)
    {
        const int NX = 36, NY = 12, it = iters();
        double v[NY][NX];
        const double view = twin ? 4.0 : 1.0;
        const double hw = 0.42 * asp * scale * view, hh = 0.21 * scale * view;
        double lo = 1e9;
        for (int j = 0; j < NY; ++j)
            for (int i = 0; i < NX; ++i) {
                v[j][i] = escape(cx + hw * (2.0 * i / (NX - 1) - 1), cy + hh * (2.0 * j / (NY - 1) - 1), it);
                if (v[j][i] >= 0) lo = std::min(lo, v[j][i]);
            }
        base = lo < 1e9 ? (float)lo : 0.0f;
        double best = 0; int bi = NX / 2, bj = NY / 2;
        for (int j = 1; j < NY - 1; ++j)
            for (int i = 1; i < NX - 1; ++i) {
                double s = 0; int inside = v[j][i] < 0;
                const int di[4] = {1, -1, 0, 0}, dj[4] = {0, 0, 1, -1};
                for (int n = 0; n < 4; ++n) {
                    const double a = v[j][i], b = v[j + dj[n]][i + di[n]];
                    if ((a < 0) != (b < 0)) s += 3;                                // the set's edge: the richest place
                    else if (a >= 0) s += std::fabs(std::log1p(a) - std::log1p(b)) * 10;  // escape counts churning
                    inside += b < 0;
                }
                if (inside == 5) continue;                                         // deep black: nothing to see
                const double dx = (i - (NX - 1) * 0.5) / NX, dy = (j - (NY - 1) * 0.5) / NY;
                s *= 1.0 - 0.6 * std::sqrt(dx * dx + dy * dy);
                if (s > best) { best = s; bi = i; bj = j; }
            }
        if (best < 0.5) { leaving = true; return; }                               // nothing left worth diving into
        tx = cx + hw * (2.0 * bi / (NX - 1) - 1);
        ty = cy + hh * (2.0 * bj / (NY - 1) - 1);
    }

    void update(float dt, int k, double asp, double time, double mid)
    {
        if (k != kind) { kind = k; restart(); fade = 0; }
        if (leaving) {
            fade = std::max(0.0f, fade - dt / 1.2f);
            if (fade <= 0) restart();
            else { scale *= std::exp(-kRate * speed * dt); return; }                      // keep falling while it fades
        }
        fade = std::min(1.0f, fade + dt / 1.2f);
        const double wob = 0.06 * std::min(1.0, scale / home);                    // deep down the shape is too sensitive to wobble
        jx = -0.7269 + wob * std::sin(time * 0.4 + mid);
        jy = 0.1889 + wob * std::cos(time * 0.33);
        scale *= std::exp(-kRate * speed * dt);
        const double pull = 1.0 - std::exp(-dt / 1.1);
        cx += (tx - cx) * pull; cy += (ty - cy) * pull;
        if ((searchIn -= dt) <= 0) { searchIn = 0.3f; const double ox = tx, oy = ty; search(asp); if (!seek) { tx = ox; ty = oy; } }
        if (scale < kMinScale) leaving = true;
    }
};
