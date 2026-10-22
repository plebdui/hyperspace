#pragma once
#include "FractalLogo.hpp"
#include <atomic>
#include <chrono>
#include <cmath>
#include <exception>
#include <functional>
#include <stdexcept>
#include <thread>

// A software splash remains responsive while the worker owns the OpenGL
// context. Drawing the splash never waits for the shader compiler or GPU.
static void loadWithScreen(SDL_Window* window, SDL_GLContext context,
                           const std::function<void(std::atomic<int>&)>& work,
                           const char* const* stages, int count, bool visible)
{
    std::atomic<int> stage{0};
    if (!visible) { work(stage); return; }
    SDL_Window* splash = SDL_CreateWindow("Hyperspace / loading", 640, 360, 0);
    setAppIcon(splash);
    SDL_Renderer* renderer = splash ? SDL_CreateRenderer(splash, "software") : nullptr;
    if (!renderer) {
        if (splash) SDL_DestroyWindow(splash);
        throw std::runtime_error(std::string("Could not create loading screen: ") + SDL_GetError());
    }
    if (!SDL_GL_MakeCurrent(window, nullptr)) {
        SDL_DestroyRenderer(renderer); SDL_DestroyWindow(splash);
        throw std::runtime_error("Could not release the rendering context for loading");
    }
    std::atomic<bool> done{false};
    std::exception_ptr failure;
    std::thread worker([&] {
        try {
            if (!SDL_GL_MakeCurrent(window, context))
                throw std::runtime_error("Could not activate the rendering context for loading");
            work(stage);
            glFinish();
        } catch (...) { failure = std::current_exception(); }
        SDL_GL_MakeCurrent(window, nullptr);
        done.store(true);
    });
    const auto start = std::chrono::steady_clock::now();
    do {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT || e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED ||
                (e.type == SDL_EVENT_KEY_DOWN && (e.key.key == SDLK_ESCAPE || e.key.key == SDLK_Q))) {
                // A driver compile cannot be interrupted safely. Explicit quit
                // exits this process instead of hanging on a worker join.
                std::exit(0);
            }
        }
        const float seconds = std::chrono::duration<float>(std::chrono::steady_clock::now()-start).count();
        SDL_SetRenderDrawColor(renderer, 8, 5, 20, 255); SDL_RenderClear(renderer);
        SDL_SetRenderDrawColor(renderer, 170, 196, 255, 255);
        for (int ring=0; ring<2; ++ring) {
            const auto points=kochPoints(ImVec2(320,94),ring==0?48.0f:24.0f,
                seconds*(ring==0?0.25f:-0.4f),2);
            std::vector<SDL_FPoint> line;
            for (const auto& p : points) line.push_back({p.x,p.y});
            line.push_back(line.front());
            SDL_RenderLines(renderer,line.data(),(int)line.size());
        }
        SDL_FRect center{318,92,4,4};
        SDL_RenderFillRect(renderer,&center);
        SDL_SetRenderScale(renderer, 2, 2);
        SDL_SetRenderDrawColor(renderer, 236, 221, 255, 255);
        SDL_RenderDebugText(renderer, 120, 83, "HYPERSPACE");
        SDL_SetRenderScale(renderer, 1, 1);
        const int current = std::min(stage.load(), count-1);
        SDL_SetRenderDrawColor(renderer, 180, 164, 215, 255);
        const char* label=stages[current];
        SDL_RenderDebugText(renderer, (640-std::strlen(label)*8)/2.0f, 214, label);
        SDL_FRect track{80,247,480,3};
        SDL_SetRenderDrawColor(renderer, 40, 29, 65, 255); SDL_RenderFillRect(renderer,&track);
        track.w=480*float(current)/count;
        SDL_SetRenderDrawColor(renderer, 175, 112, 250, 255); SDL_RenderFillRect(renderer,&track);
        char detail[128];
        std::snprintf(detail,sizeof(detail),"Stage %d / %d   |   %.0fs elapsed",current+1,count,seconds);
        SDL_RenderDebugText(renderer,(640-std::strlen(detail)*8)/2.0f,271,detail);
        SDL_SetRenderDrawColor(renderer, 126, 113, 155, 255);
        const char* note="Compiling / warming shaders. Esc to cancel.";
        SDL_RenderDebugText(renderer,(640-std::strlen(note)*8)/2.0f,307,note);
        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    } while (!done.load());
    worker.join();
    SDL_DestroyRenderer(renderer); SDL_DestroyWindow(splash);
    if (!SDL_GL_MakeCurrent(window, context)) throw std::runtime_error("Could not restore rendering context");
    if (failure) std::rethrow_exception(failure);
}
