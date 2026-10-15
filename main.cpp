// hyperspace (working name): a DMT-style audio visualizer.
// Listens to whatever your speakers are playing and takes you on a trip:
// come-up, chrysanthemum, breakthrough, hyperspace, where entities appear.
// The music's energy carries you deeper, drops push you through, quiet
// brings you back. Drop a picture, GIF or video on the window and the trip
// starts over from it: it melts into the come-up, folds into the mandala,
// papers the tunnel and tiles the dome, its colours colour everything, and
// the same file always takes the same trip. See --help for keys and options.
//
// Each frame: the SCENE shader draws the stage (with the last frame melting
// under it), the WIREFRAMES are drawn on top (so they leave trails too), FX
// mirrors it / traces its edges / adds contrast, BLOOM makes the glow, and
// LOOK does the film tricks (a picture effect and a camera move, picked by
// the music) and the shatters, ripples and fireworks, and PRESENT puts it on screen.
#include <SDL3/SDL.h>
#include <epoxy/gl.h>

#include "Audio.hpp"
#include "LookShader.hpp"
#include "Looks.hpp"
#include "Media.hpp"
#include "Shaders.hpp"
#include "Wire.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static const char* const kHelp = R"(hyperspace: a DMT-style audio visualizer

  hyperspace            listen to what your speakers are playing
  hyperspace --mic      listen to the microphone instead
  hyperspace --demo     play its own built-in track (silently; it just listens to it)
  --media file          start the trip from a picture, GIF or video (or drop one on the window)
  --fullscreen          start fullscreen
  --scale 0.75          how sharp, 0.3-1 (lower is faster)

keys:
  F / F11   fullscreen            1 2 3 4   hold the trip at a stage:
  M         next mirror                     come-up, chrysanthemum, breakthrough, hyperspace
  P         next pattern          E         next entity
  V         next picture effect   C         one camera move at a time (0: back to the stack)
  0         hand it all back to the music
  [ ]       less / more sharp     Esc / Q   quit
)";

static const int kNumPatterns = 6, kNumMirrors = 6;
static const char* const kPatternNames[kNumPatterns] = {"flower of life", "truchet weave", "op-art", "moire", "julia", "honeycomb"};
static const char* const kMirrorNames[kNumMirrors] = {"no mirror", "left/right", "four ways", "kaleido 6", "kaleido 8", "kaleido 12"};

static GLuint compile(GLenum type, const char* src)
{
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        std::fprintf(stderr, "shader didn't compile:\n%s\n", log);
        std::exit(1);
    }
    return s;
}
static GLuint program(const char* frag, const char* vert = kVertex)
{
    GLuint p = glCreateProgram();
    glAttachShader(p, compile(GL_VERTEX_SHADER, vert));
    glAttachShader(p, compile(GL_FRAGMENT_SHADER, frag));
    glLinkProgram(p);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        std::fprintf(stderr, "shaders didn't link\n");
        std::exit(1);
    }
    return p;
}

// a picture to draw into
struct Target {
    GLuint tex = 0, fbo = 0;
    int w = 0, h = 0;
    void make(int width, int height, GLint wrap = GL_CLAMP_TO_EDGE)
    {
        if (tex) { glDeleteTextures(1, &tex); glDeleteFramebuffers(1, &fbo); }
        w = width;
        h = height;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    void bind() const
    {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(0, 0, w, h);
    }
};

// what changes on drops (or every so often): the pattern (crossfaded) and the mirror (a cut)
struct Show {
    int patA = 0, patB = 1, mirror = 0;
    float patT = 0, sincePat = 0, sinceMirror = 0;
    bool fading = false;
    int forcedMirror = -1, forcedPattern = -1;
    uint32_t rng = 12345;

    int roll(int n) { rng = rng * 1664525u + 1013904223u; return (int)((rng >> 8) % (uint32_t)n); }
    void update(const Features& f, float stage, float dt)
    {
        sincePat += dt;
        sinceMirror += dt;
        if (forcedPattern >= 0 && forcedPattern != patA && !fading) { patB = forcedPattern; fading = true; }
        else if (forcedPattern < 0 && !fading && ((f.drop && sincePat > 6) || sincePat > 18)) {
            patB = (patA + 1 + roll(kNumPatterns - 1)) % kNumPatterns;
            fading = true;
        }
        if (fading) {
            patT += dt / 2.5f;
            if (patT >= 1) { patA = patB; patT = 0; fading = false; sincePat = 0; }
        }
        if (forcedMirror >= 0) mirror = forcedMirror;
        else if ((f.drop && sinceMirror > 4) || sinceMirror > 24) {
            // deeper in the trip, more folds
            static const int early[] = {0, 1}, mid[] = {1, 2, 3}, deep[] = {2, 3, 4, 5};
            if (stage < 0.8f) mirror = early[roll(2)];
            else if (stage < 1.8f) mirror = mid[roll(3)];
            else mirror = deep[roll(4)];
            sinceMirror = 0;
        }
    }
};

// testing: save the window as a picture
static void saveShot(const std::string& path, int w, int h)
{
    // read as floats and convert here: reading the (half-float) picture straight into bytes came back
    // all black at larger sizes on this driver
    std::vector<float> fl((size_t)w * h * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_FLOAT, fl.data());
    std::vector<unsigned char> px((size_t)w * h * 3);
    for (size_t i = 0, n = (size_t)w * h; i < n; ++i)
        for (int c = 0; c < 3; ++c) {
            const float v = fl[i * 4 + c];
            px[i * 3 + c] = (unsigned char)std::lround(std::clamp(std::isfinite(v) ? v : 0.0f, 0.0f, 1.0f) * 255);
        }
    if (FILE* f = std::fopen(path.c_str(), "wb")) {
        std::fprintf(f, "P6 %d %d 255\n", w, h);
        for (int y = h - 1; y >= 0; --y) std::fwrite(&px[(size_t)y * w * 3], 1, (size_t)w * 3, f);
        std::fclose(f);
    }
}

static float weight(float stage, int k) { return std::clamp(1 - std::fabs(stage - k), 0.0f, 1.0f); }

int main(int argc, char** argv)
{
    Capture::Source src = Capture::kSpeakers;
    bool fullscreen = false;
    float scale = 0.75f;
    int forceStage = -1, forceEntity = -1, forceMirror = -1, forcePattern = -1, forceFx = -1, forceCam = -1;
    std::string shotPath, mediaPath;  // testing: --shot file --at seconds (then quit)
    float shotAt = 5;
    int width = 1280, height = 720;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() { return i + 1 < argc ? argv[++i] : ""; };
        if (a == "--mic") src = Capture::kMic;
        else if (a == "--demo") src = Capture::kDemo;
        else if (a == "--fullscreen") fullscreen = true;
        else if (a == "--scale") scale = std::clamp((float)std::atof(next()), 0.3f, 1.0f);
        else if (a == "--stage") forceStage = std::atoi(next());
        else if (a == "--entity") forceEntity = std::atoi(next());
        else if (a == "--mirror") forceMirror = std::atoi(next());
        else if (a == "--pattern") forcePattern = std::atoi(next());
        else if (a == "--fx") forceFx = std::atoi(next()) % kNumFx;
        else if (a == "--cam") forceCam = std::atoi(next()) % kNumCams;
        else if (a == "--shot") shotPath = next();
        else if (a == "--media") mediaPath = next();
        else if (a == "--at") shotAt = (float)std::atof(next());
        else if (a == "--size") std::sscanf(next(), "%dx%d", &width, &height);
        else { std::fputs(kHelp, a == "--help" || a == "-h" ? stdout : stderr); return a == "--help" || a == "-h" ? 0 : 1; }
    }

    Capture cap;
    if (!cap.start(src)) {
        std::fprintf(stderr, "couldn't listen to the sound (%s); playing the demo track instead\n", cap.why().c_str());
        cap.stop();
        cap.start(Capture::kDemo);
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL: %s\n", SDL_GetError());
        return 1;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window* win = SDL_CreateWindow("hyperspace", width, height,
                                       SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | (fullscreen ? SDL_WINDOW_FULLSCREEN : 0)
                                       | (shotPath.empty() ? 0 : SDL_WINDOW_HIDDEN));  // (testing: never shown, drawn off-screen)
    if (!win || !SDL_GL_CreateContext(win)) {
        std::fprintf(stderr, "couldn't open a window: %s\n", SDL_GetError());
        return 1;
    }
    SDL_GL_SetSwapInterval(std::getenv("HYPERSPACE_NOVSYNC") || !shotPath.empty() ? 0 : 1);  // (testing: measure how fast it can go)
    if (shotPath.empty()) std::fputs(kHelp + std::strlen("hyperspace: a DMT-style audio visualizer\n"), stdout);

    GLuint vao;
    glGenVertexArrays(1, &vao);
    const GLuint scene = program(kScene), fx = program(kFx), bright = program(kBright), blur = program(kBlur),
                 present = program(kPresent), wire = program(kWireFragment, kWireVertex), look = program(kLook),
                 copy = program(kCopy);
    // the wireframes' strips
    GLuint wireVao, wireVbo;
    glGenVertexArrays(1, &wireVao);
    glGenBuffers(1, &wireVbo);
    glBindVertexArray(wireVao);
    glBindBuffer(GL_ARRAY_BUFFER, wireVbo);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(2 * sizeof(float)));
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    for (int i = 0; i < 3; ++i) glEnableVertexAttribArray(i);

    Target sceneT[2], fxT, lookT, frozenT, bloomT[2], shotT;  // (shotT: testing, the screen drawn off-screen)
    int cur = 0;
    // the history: the last kHist pictures (at half size, 30 a second) for the effects that reach back in time
    const int kHist = 48;
    GLuint histTex, histFbo;
    glGenTextures(1, &histTex);
    glGenFramebuffers(1, &histFbo);
    int histW = 0, histH = 0, head = 0;
    float histClock = 0;

    Analysis analysis;
    Trip trip;
    trip.forcedStage = forceStage;
    trip.forcedEntity = forceEntity;
    Show show;
    show.forcedMirror = forceMirror;
    show.forcedPattern = forcePattern;
    if (forcePattern >= 0) show.patA = forcePattern;
    Looks looks;
    looks.forcedFx = forceFx;
    looks.forcedCam = forceCam;
    looks.forceFire = std::getenv("HYPERSPACE_FIREWORKS") != nullptr;
    float gridX = 0, gridZ = 0;
    // a dropped picture: its frames on the GPU, its colours, and how far it's faded in
    Media media;
    GLuint mediaTex;
    glGenTextures(1, &mediaTex);
    glBindTexture(GL_TEXTURE_2D, mediaTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, Media::kW, Media::kH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
    std::vector<uint8_t> mediaFrame;
    float seedPal[3][3] = {}, seedMix = 0, mediaAmt = 0, mediaAge = 0;
    bool gotFrame = false;
    Wires wires;
    std::vector<float> samples(Analysis::kN);
    std::string dropped = mediaPath;
    float clock = 0, time = 0, hue = 0, zoom = 0, spin = 0, status = 0, turn4[6] = {0, 0, 0, 0, 0, 0};
    uint64_t last = SDL_GetTicksNS();
    int frames = 0;
    bool run = true;
    while (run) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) run = false;
            if (e.type == SDL_EVENT_DROP_FILE && e.drop.data) dropped = e.drop.data;
            if (e.type != SDL_EVENT_KEY_DOWN) continue;
            const SDL_Keycode k = e.key.key;
            if (k == SDLK_ESCAPE || k == SDLK_Q) run = false;
            else if (k == SDLK_F || k == SDLK_F11) { fullscreen = !fullscreen; SDL_SetWindowFullscreen(win, fullscreen); }
            else if (k >= SDLK_1 && k <= SDLK_4) trip.forcedStage = (int)(k - SDLK_1);
            else if (k == SDLK_0) trip.forcedStage = trip.forcedEntity = show.forcedMirror = show.forcedPattern = looks.forcedFx = looks.forcedCam = -1;
            else if (k == SDLK_V) looks.forcedFx = (looks.fx + 1) % kNumFx;
            else if (k == SDLK_C) looks.forcedCam = (looks.forcedCam < 0 ? 1 : looks.forcedCam + 1) % kNumCams;
            else if (k == SDLK_E) trip.forcedEntity = (trip.entity + 1) % kNumEntities;
            else if (k == SDLK_M) show.forcedMirror = (show.mirror + 1) % kNumMirrors;
            else if (k == SDLK_P) show.forcedPattern = ((show.fading ? show.patB : show.patA) + 1) % kNumPatterns;
            else if (k == SDLK_LEFTBRACKET) scale = std::max(0.3f, scale - 0.1f);
            else if (k == SDLK_RIGHTBRACKET) scale = std::min(1.0f, scale + 0.1f);
        }
        const uint64_t now = SDL_GetTicksNS();
        const float dt = std::min((now - last) / 1e9f, 0.1f);
        last = now;
        clock += dt;

        // a picture dropped in: the trip starts over from it, its bytes setting every choice it makes
        if (!dropped.empty()) {
            if (media.load(dropped)) {
                trip.restart();
                show.rng = media.seed;
                looks.rng = media.seed * 2654435761u + 1;
                hue = (media.seed % 1000) / 1000.0f;
                mediaAge = 0;
                seedMix = 0;
                gotFrame = false;
                std::printf("\n  starting from %s\n", media.name.c_str());
            } else {
                std::printf("\n  couldn't find a picture in %s\n", dropped.c_str());
            }
            dropped.clear();
        }
        if (media.hasPicture) {
            mediaAge += dt;
            if (media.frame(mediaFrame)) {
                glBindTexture(GL_TEXTURE_2D, mediaTex);
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, Media::kW, Media::kH, GL_RGBA, GL_UNSIGNED_BYTE, mediaFrame.data());
                gotFrame = true;
            }
            if (media.palette(seedPal)) seedMix = std::min(1.0f, seedMix + dt / 6);  // its colours seep in
            mediaAmt = gotFrame ? std::min(1.0f, mediaAmt + dt * 2) : 0;
        }
        // the first seconds after a drop belong to the picture; then the trip takes it over
        const float introT = std::clamp((mediaAge - 2.5f) / 4.5f, 0.0f, 1.0f);
        const float intro = media.hasPicture ? 1 - introT * introT * (3 - 2 * introT) : 0.0f;

        cap.latest(samples.data(), Analysis::kN);
        const Features f = analysis.update(samples.data(), dt);
        trip.update(f, dt);
        show.update(f, trip.stage, dt);
        looks.update(f, trip.stage, clock, dt);
        if (const char* at = std::getenv("HYPERSPACE_SHATTER_AT"); at && clock >= std::atof(at) && clock - dt < std::atof(at)) {
            looks.shatterT = 0;  // (testing: shatter now)
            if (const char* k = std::getenv("HYPERSPACE_BREAK")) looks.breakKind = std::atoi(k);
            looks.sinceShatter = 0;
            looks.grabNow = true;
        }
        const float sdt = dt * looks.timeScale;  // the trip's own time: slowed by slow motion and bullet time
        time += sdt;
        hue += sdt * (0.015f + 0.05f * f.mid);
        zoom += sdt * (0.25f + 0.9f * f.bass) * (0.3f + weight(trip.stage, 2));
        spin += sdt * (0.04f + 0.15f * f.high);
        for (int k = 0; k < 6; ++k) turn4[k] += sdt * (0.05f + 0.07f * k) * (0.6f + 0.6f * f.mid + 0.8f * f.beat);
        const float gridSpeed = sdt * (0.8f + 2.0f * f.bass);  // walking the grid, the way it faces
        gridX += std::sin(looks.gridTurn) * gridSpeed;
        gridZ += std::cos(looks.gridTurn) * gridSpeed;

        int ww, wh;
        SDL_GetWindowSizeInPixels(win, &ww, &wh);
        if (!shotPath.empty()) { ww = width; wh = height; }  // (testing: the off-screen size)
        const int sw = std::max(64, (int)(ww * scale)), sh = std::max(64, (int)(wh * scale));
        if (sceneT[0].w != sw || sceneT[0].h != sh) {
            sceneT[0].make(sw, sh);
            sceneT[1].make(sw, sh);
            fxT.make(sw, sh, GL_MIRRORED_REPEAT);
            lookT.make(sw, sh);
            frozenT.make(sw, sh, GL_MIRRORED_REPEAT);
            histW = std::max(32, sw / 2);
            histH = std::max(32, sh / 2);
            glBindTexture(GL_TEXTURE_2D_ARRAY, histTex);
            glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, histW, histH, kHist, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
            glBindFramebuffer(GL_FRAMEBUFFER, histFbo);
            for (int l = 0; l < kHist; ++l) {
                glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, histTex, 0, l);
                glClear(GL_COLOR_BUFFER_BIT);
            }
            bloomT[0].make(sw / 2, sh / 2);
            bloomT[1].make(sw / 2, sh / 2);
        }
        const float bass = std::min(f.bass, 2.5f) * 0.6f, mid = std::min(f.mid, 2.5f) * 0.6f, high = std::min(f.high, 2.5f) * 0.6f;
        auto U = [&](GLuint p, const char* n) { return glGetUniformLocation(p, n); };

        // ---- the scene, reading the last frame
        const int nxt = cur ^ 1;
        sceneT[nxt].bind();
        glBindVertexArray(vao);
        glUseProgram(scene);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, sceneT[cur].tex);
        glUniform1i(U(scene, "uPrev"), 0);
        glUniform2f(U(scene, "uRes"), (float)sw, (float)sh);
        glUniform1f(U(scene, "uTime"), time);
        glUniform1f(U(scene, "uBass"), bass);
        glUniform1f(U(scene, "uMid"), mid);
        glUniform1f(U(scene, "uHigh"), high);
        glUniform1f(U(scene, "uLevel"), f.level);
        glUniform1f(U(scene, "uBeat"), f.beat);
        glUniform1f(U(scene, "uStage"), trip.stage);
        glUniform1f(U(scene, "uEntityAmt"), trip.entityAmt);
        glUniform1i(U(scene, "uEntity"), trip.entity);
        glUniform1f(U(scene, "uMood"), f.mood);
        glUniform1f(U(scene, "uHue"), hue);
        glUniform1f(U(scene, "uZoom"), zoom);
        glUniform1f(U(scene, "uSpin"), spin);
        glUniform1i(U(scene, "uPatA"), show.patA);
        glUniform1i(U(scene, "uPatB"), show.patB);
        glUniform1f(U(scene, "uPatT"), show.patT);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, mediaTex);
        glActiveTexture(GL_TEXTURE0);
        glUniform1i(U(scene, "uMedia"), 1);
        glUniform1f(U(scene, "uMediaAmt"), mediaAmt);
        glUniform1f(U(scene, "uMediaAge"), mediaAge);
        glUniform3fv(U(scene, "uSeedPal"), 3, &seedPal[0][0]);
        glUniform1f(U(scene, "uSeedMix"), seedMix);
        glUniform1f(U(scene, "uGridTurn"), looks.gridTurn);
        glUniform2f(U(scene, "uGridPos"), gridX, gridZ);
        glUniform1f(U(scene, "uGridFocus"), std::clamp(trip.stage / 2.5f + 0.2f * f.level, 0.0f, 1.0f));
        glUniform1f(U(scene, "uPatAmt"), (0.35f + 0.25f * std::min(trip.stage, 2.0f) / 2 + 0.2f * f.beat) * (1 - intro));
        glUniform1f(U(scene, "uIntro"), intro);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        cur = nxt;

        // ---- the wireframes, added on top (they'll trail in the next frames)
        wires.clear();
        wires.aspect = (float)sw / sh;
        const float w0 = weight(trip.stage, 0), w1 = weight(trip.stage, 1), w2 = weight(trip.stage, 2), w3 = weight(trip.stage, 3);
        const float breathe = 1 + 0.12f * bass + 0.1f * f.beat;
        wires.metatron(0, 0, 0.17f * breathe, spin * 0.5f, palette(hue + 0.4f, f.mood), 0.35f * w0 + 0.7f * w1);
        static const int midShapes[3] = {14, 13, 17};  // icosahedron, dodecahedron, tesseract
        wires.polytope(midShapes[show.patA % 3], 0, 0, 0.36f * breathe, turn4, palette(hue + 0.1f, f.mood), 0.8f * w1);
        wires.tunnel(6 + (show.patA % 3) * 2, zoom * 0.5f, spin, hue, f.mood, 1.0f * w2);
        if (w3 > 0) {
            wires.polytope(trip.entity == kJesters ? 19 : 17, 0, 0, 0.5f * breathe, turn4, palette(hue + 0.6f, f.mood), 0.55f * w3, 0.005f);
            for (int i = 0; i < 6; ++i) {  // a ring of small hypercubes, turning
                const float an = spin * 0.6f + i * 1.0471976f;
                float t2[6];
                for (int k = 0; k < 6; ++k) t2[k] = turn4[k] * (1 + 0.2f * i) + i;
                wires.polytope(i % 2 ? 18 : 17, std::cos(an) * 0.72f, std::sin(an) * 0.4f, 0.08f * breathe, t2, palette(hue + i * 0.12f, f.mood), 0.7f * w3);
            }
        }
        if (!wires.data.empty()) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE);
            glUseProgram(wire);
            glBindVertexArray(wireVao);
            glBindBuffer(GL_ARRAY_BUFFER, wireVbo);
            glBufferData(GL_ARRAY_BUFFER, wires.data.size() * sizeof(float), wires.data.data(), GL_STREAM_DRAW);
            glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(wires.data.size() / 6));
            glDisable(GL_BLEND);
            glBindVertexArray(vao);
        }

        // ---- FX: mirror, trace the edges, contrast
        fxT.bind();
        glUseProgram(fx);
        glBindTexture(GL_TEXTURE_2D, sceneT[cur].tex);
        glUniform1i(U(fx, "uScene"), 0);
        glUniform2f(U(fx, "uRes"), (float)sw, (float)sh);
        glUniform1i(U(fx, "uMirror"), intro > 0.3f ? 0 : show.mirror);
        glUniform1f(U(fx, "uTrace"), (0.35f + 0.25f * trip.stage / 3 + 0.4f * high) * (1 - 0.8f * intro));
        glUniform1f(U(fx, "uContrast"), (0.35f + 0.45f * trip.stage / 3) * (1 - intro));
        glUniform1f(U(fx, "uHue"), hue);
        glUniform1f(U(fx, "uMood"), f.mood);
        glUniform1f(U(fx, "uTurn"), spin * 0.3f);
        glUniform3fv(U(fx, "uSeedPal"), 3, &seedPal[0][0]);
        glUniform1f(U(fx, "uSeedMix"), seedMix);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        // ---- into the history (30 a second), and kept whole when the look needs a still
        histClock += dt;
        if (histClock >= 1.0f / 30) {
            histClock = std::fmod(histClock, 1.0f / 30);
            head = (head + 1) % kHist;
            glBindFramebuffer(GL_FRAMEBUFFER, histFbo);
            glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, histTex, 0, head);
            glViewport(0, 0, histW, histH);
            glUseProgram(copy);
            glBindTexture(GL_TEXTURE_2D, fxT.tex);
            glUniform1i(U(copy, "uSrc"), 0);
            glUniform2f(U(copy, "uRes"), (float)histW, (float)histH);
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }
        if (looks.grabNow) {
            glBindFramebuffer(GL_READ_FRAMEBUFFER, fxT.fbo);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, frozenT.fbo);
            glBlitFramebuffer(0, 0, sw, sh, 0, 0, sw, sh, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        }

        // ---- LOOK: the camera move, the picture effect, and the shatters / ripples / fireworks
        lookT.bind();
        glUseProgram(look);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, frozenT.tex);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D_ARRAY, histTex);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, fxT.tex);
        glUniform1i(U(look, "uScene"), 0);
        glUniform1i(U(look, "uHist"), 1);
        glUniform1i(U(look, "uFrozen"), 2);
        glUniform1i(U(look, "uHead"), head);
        static const int kNoCams[Looks::kStack] = {};
        glUniform1i(U(look, "uFx"), intro > 0.3f ? 0 : looks.fx);  // no film tricks while the picture is still itself
        glUniform1iv(U(look, "uCams"), Looks::kStack, intro > 0.3f ? kNoCams : looks.cams);
        glUniform1fv(U(look, "uCamTs"), Looks::kStack, looks.camTs);
        glUniform2f(U(look, "uRes"), (float)sw, (float)sh);
        glUniform1f(U(look, "uTime"), clock);
        glUniform1f(U(look, "uBass"), bass);
        glUniform1f(U(look, "uMid"), mid);
        glUniform1f(U(look, "uHigh"), high);
        glUniform1f(U(look, "uLevel"), f.level);
        glUniform1f(U(look, "uBeat"), f.beat);
        glUniform1f(U(look, "uHue"), hue);
        glUniform1f(U(look, "uMood"), f.mood);
        glUniform1f(U(look, "uFxT"), looks.fxT);
        glUniform1f(U(look, "uSpeed"), looks.speed);
        glUniform1f(U(look, "uShatterT"), looks.shatterT);
        glUniform1f(U(look, "uShatterSeed"), looks.shatterSeed);
        glUniform1f(U(look, "uFigures"), looks.figures);
        glUniform1f(U(look, "uCloak"), looks.cloak);
        glUniform1f(U(look, "uStage"), trip.stage);
        glUniform1f(U(look, "uIntro"), intro);
        glUniform1f(U(look, "uBreakKind"), (float)looks.breakKind);
        glUniform1f(U(look, "uWatchers"), looks.watchers);
        glUniform3fv(U(look, "uSeedPal"), 3, &seedPal[0][0]);
        glUniform1f(U(look, "uSeedMix"), seedMix);
        glUniform4fv(U(look, "uBursts"), Looks::kBursts, &looks.bursts[0][0]);
        glUniform3fv(U(look, "uRipples"), 6, &looks.ripples[0][0]);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        // ---- the glow: the bright parts at half size, smeared across then down (twice, for a wide glow)
        bloomT[0].bind();
        glUseProgram(bright);
        glBindTexture(GL_TEXTURE_2D, lookT.tex);
        glUniform1i(U(bright, "uSrc"), 0);
        glUniform2f(U(bright, "uRes"), (float)bloomT[0].w, (float)bloomT[0].h);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glUseProgram(blur);
        glUniform1i(U(blur, "uSrc"), 0);
        glUniform2f(U(blur, "uRes"), (float)bloomT[0].w, (float)bloomT[0].h);
        for (int pass = 0; pass < 4; ++pass) {
            const int from = pass & 1;
            bloomT[from ^ 1].bind();
            glBindTexture(GL_TEXTURE_2D, bloomT[from].tex);
            glUniform2f(U(blur, "uDir"), pass & 1 ? 0.0f : 1.0f / bloomT[0].w, pass & 1 ? 1.0f / bloomT[0].h : 0.0f);
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }

        // ---- onto the screen
        if (shotPath.empty()) glBindFramebuffer(GL_FRAMEBUFFER, 0);
        else {
            if (shotT.w != width || shotT.h != height) shotT.make(width, height);
            glBindFramebuffer(GL_FRAMEBUFFER, shotT.fbo);
        }
        glViewport(0, 0, ww, wh);
        glUseProgram(present);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, bloomT[0].tex);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, lookT.tex);
        glUniform1i(U(present, "uScene"), 0);
        glUniform1i(U(present, "uBloom"), 1);
        glUniform2f(U(present, "uRes"), (float)ww, (float)wh);
        glUniform1f(U(present, "uBass"), bass);
        glUniform1f(U(present, "uTime"), time);
        glUniform1f(U(present, "uBloomAmt"), (0.6f + 0.5f * trip.stage / 3 + 0.4f * f.beat) * (1 - 0.7f * intro));  // a pale picture shouldn't burn out
        glDrawArrays(GL_TRIANGLES, 0, 3);

        if (!shotPath.empty() && clock >= shotAt) {
            glBindFramebuffer(GL_FRAMEBUFFER, shotT.fbo);  // read back the finished picture (without this it sometimes came back black)
            glFinish();
            saveShot(shotPath, ww, wh);
            std::printf("saved (%s, %s %.0f%%, %s, %s, %s, %s, %.0f fps)\n", kStageNames[std::clamp((int)std::lround(trip.stage), 0, 3)],
                        kEntityNames[trip.entity], trip.entityAmt * 100, kPatternNames[show.patA], kMirrorNames[show.mirror],
                        kFxNames[looks.fx], looks.camNames().c_str(), frames / clock);
            run = false;
        }
        if (shotPath.empty()) SDL_GL_SwapWindow(win);
        else SDL_Delay(3);  // (testing: hidden, nothing holds it to the screen's rate; thousands of frames a second turn the picture black)
        ++frames;

        status += dt;
        if (status > 0.5f && shotPath.empty()) {
            status = 0;
            std::printf("\r  %-14s %-12s %-15s %-11s %-16s %-44s %s %-6s %3.0f fps   ",
                        kStageNames[std::clamp((int)std::lround(trip.stage), 0, 3)],
                        trip.entityAmt > 0.05f ? kEntityNames[trip.entity] : "", kPatternNames[show.patA], kMirrorNames[show.mirror],
                        kFxNames[looks.fx], looks.camNames().c_str(), trip.forcedStage >= 0 ? "(held)" : "      ",
                        f.mood > 0.55f ? "bright" : "dark", frames / clock);
            std::fflush(stdout);
        }
    }
    std::printf("\n");
    cap.stop();
    SDL_Quit();
    return 0;
}
