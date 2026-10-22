#pragma once
#include <SDL3/SDL.h>
#include "imgui.h"
#include <vector>
#include <cmath>

// Shared geometry for the settings button and the loading logo.
static std::vector<ImVec2> kochPoints(ImVec2 c, float r, float rot, int depth)
{
    std::vector<ImVec2> pts;
    for (int i = 0; i < 3; ++i) {
        const float a = rot + i * 2.0943951f - 1.5707963f;  // point one corner up
        pts.push_back(ImVec2(c.x + std::cos(a) * r, c.y + std::sin(a) * r));
    }
    auto d2 = [&](ImVec2 p) { return (p.x - c.x) * (p.x - c.x) + (p.y - c.y) * (p.y - c.y); };
    for (int it = 0; it < depth; ++it) {
        std::vector<ImVec2> np;
        const int n = (int)pts.size();
        for (int i = 0; i < n; ++i) {
            const ImVec2 A = pts[i], B = pts[(i + 1) % n];
            const ImVec2 d((B.x - A.x) / 3.0f, (B.y - A.y) / 3.0f);
            const ImVec2 p1(A.x + d.x, A.y + d.y), p2(A.x + 2 * d.x, A.y + 2 * d.y);
            const float cs = 0.5f, sn = 0.8660254f;  // rotate the middle third by 60 deg to raise the spike
            ImVec2 peak(p1.x + d.x * cs - d.y * sn, p1.y + d.x * sn + d.y * cs);
            if (d2(peak) < d2(p1)) peak = ImVec2(p1.x + d.x * cs + d.y * sn, p1.y - d.x * sn + d.y * cs);  // keep spikes pointing outward
            np.push_back(A); np.push_back(p1); np.push_back(peak); np.push_back(p2);
        }
        pts.swap(np);
    }
    return pts;
}

// The same two snowflakes at rest, for the desktop and window icons.
static SDL_Surface* createAppIcon()
{
    SDL_Surface* surface = SDL_CreateSurface(256, 256, SDL_PIXELFORMAT_RGBA32);
    if (!surface) return nullptr;
    SDL_Renderer* renderer = SDL_CreateSoftwareRenderer(surface);
    if (!renderer) { SDL_DestroySurface(surface); return nullptr; }
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer, 170, 196, 255, 255);
    for (float radius : {100.0f, 50.0f}) {
        const auto points = kochPoints(ImVec2(128,128), radius, 0, 2);
        for (int offset=-1; offset<=1; ++offset) {
            std::vector<SDL_FPoint> line;
            for (const auto& p : points) line.push_back({p.x + offset, p.y});
            line.push_back(line.front());
            SDL_RenderLines(renderer,line.data(),(int)line.size());
            line.clear();
            for (const auto& p : points) line.push_back({p.x, p.y + offset});
            line.push_back(line.front());
            SDL_RenderLines(renderer,line.data(),(int)line.size());
        }
    }
    for (int y=-4; y<=4; ++y)
        for (int x=-4; x<=4; ++x)
            if (x*x+y*y<=16) SDL_RenderPoint(renderer,128+x,128+y);
    SDL_RenderPresent(renderer);
    SDL_DestroyRenderer(renderer);
    return surface;
}

static void setAppIcon(SDL_Window* window)
{
    if (!window) return;
    if (SDL_Surface* icon = createAppIcon()) {
        SDL_SetWindowIcon(window, icon);
        SDL_DestroySurface(icon);
    }
}
