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
// the music) and the shatters and ripples, and PRESENT puts it on screen.
#include <SDL3/SDL.h>
#include <epoxy/gl.h>

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl3.h"

#include "Audio.hpp"
#include "FracDive.hpp"
#include "Ineffable.hpp"
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

#include "Loading.hpp"

static const char* const kHelp = R"(hyperspace: a DMT-style audio visualizer

  hyperspace            listen to what your speakers are playing
  hyperspace --mic      listen to the microphone instead
  hyperspace --demo     play its own built-in track (silently; it just listens to it)
  --media file          start the trip from a picture, GIF or video (or drop one on the window)
  --classic             start with the original trip
  --dim 6 / 7 / 8 / 9    cathedral / torus knot / lotus / quasicrystal
  --fullscreen          start fullscreen
  --scale 0.75          how sharp, 0.3-1 (lower is faster)

keys:
  F / F11   fullscreen            1 2 3 4   hold the trip at a stage:
  M         next mirror                     come-up, chrysanthemum, breakthrough, hyperspace
  P         next pattern          E         next entity
  V         next picture effect   C         one camera move at a time (0: back to the stack)
  5 6 7 8 9 fly into an extra dimension: 3D fractal, fractal, pillars, forest, corridor
            (press again to leave)
  Tab       show / hide settings
  J         toggle the Ineffable journey
  N         next mathematical world (hold it)
  0         return to the original music-driven trip
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
        throw std::runtime_error(log);
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
        throw std::runtime_error("Shader program did not link");
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

// the settings sidebar's state. A slider scales each effect (1 = however the music is driving it,
// 0 = off, up to 2-3 = exaggerated); the dropdowns pin a choice instead of letting the music pick.
struct UI {
    bool show = false;     // is the sidebar slid in?
    float anim = 0;        // 0 hidden, 1 fully in — eased for the slide
    float iconFade = 0;    // the mandala icon's visibility — fades in on hover (or while open)
    float bloom = 1, trail = 1, trace = 1, contrast = 1, pattern = 1, chroma = 1,
          grain = 1, wires = 1, camMove = 1, zoomSpd = 1, spinSpd = 1, hueSpd = 1, spread = 1;
    float sacredSpeed = 1, sacredIntensity = 1, journeyHold = 28;
    // fractals
    bool fracTwin = true, fracSeek = true;
    float diveSpd = 1, f3Spd = 1, f3React = 1, f3Morph = 6, autoHold = 20;
    // trip events
    float cloak = 1, watchers = 1, onset = 1, grid = 1, entity = 1, mediaCol = 1;
    void resetEffects()
    {
        bloom = trail = trace = contrast = pattern = chroma = grain = wires = camMove = zoomSpd = spinSpd = hueSpd = spread = 1;
        sacredSpeed = sacredIntensity = 1; journeyHold = 28;
        fracTwin = fracSeek = true;
        diveSpd = f3Spd = f3React = 1; f3Morph = 6; autoHold = 20;
        cloak = watchers = onset = grid = entity = mediaCol = 1;
    }
};

// a Koch snowflake: an equilateral triangle whose every edge is recursively replaced by a smaller
// spike, a few levels deep — a real fractal. Two of them, counter-rotating, as the sidebar's toggle.
static void kochFlake(ImDrawList* dl, ImVec2 c, float r, float rot, int depth, ImU32 col, float thick)
{
    const auto pts = kochPoints(c, r, rot, depth);
    dl->AddPolyline(pts.data(), (int)pts.size(), col, ImDrawFlags_Closed, thick);
}

// the clickable fractal that pops the sidebar in and out; it warms up when you hover it
static void drawFractal(ImDrawList* dl, ImVec2 c, float r, float t, bool hot, float alpha)
{
    if (alpha <= 0.004f) return;
    const int a = (int)(std::clamp(alpha, 0.0f, 1.0f) * (hot ? 255 : 210));
    const ImU32 col = hot ? IM_COL32(255, 221, 150, a) : IM_COL32(170, 196, 255, a);
    kochFlake(dl, c, r, t * 0.25f, 2, col, 1.5f);                  // outer flake, turning slowly
    kochFlake(dl, c, r * 0.5f, -t * 0.4f, 2, col, 1.1f);          // inner flake, counter-rotating
    dl->AddCircleFilled(c, 2.0f, col);
}

int main(int argc, char** argv)
{
    Capture::Source src = Capture::kSpeakers;
    bool fullscreen = false;
    float scale = 0.75f;
    int forceStage = -1, forceEntity = -1, forceMirror = -1, forcePattern = -1, forceFx = -1, forceCam = -1, forceDim = -1;
    std::string shotPath, mediaPath;  // testing: --shot file --at seconds (then quit)
    bool classic = false;
    float shotAt = 5;
    int seqFrame = 0;  // (testing: HYPERSPACE_SEQ)
    int width = 1280, height = 720;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() { return i + 1 < argc ? argv[++i] : ""; };
        if (a == "--mic") src = Capture::kMic;
        else if (a == "--demo") src = Capture::kDemo;
        else if (a == "--classic") classic = true;
        else if (a == "--fullscreen") fullscreen = true;
        else if (a == "--scale") scale = std::clamp((float)std::atof(next()), 0.3f, 1.0f);
        else if (a == "--stage") forceStage = std::atoi(next());
        else if (a == "--entity") forceEntity = std::atoi(next());
        else if (a == "--mirror") forceMirror = std::atoi(next());
        else if (a == "--pattern") forcePattern = std::atoi(next());
        else if (a == "--fx") forceFx = std::atoi(next()) % kNumFx;
        else if (a == "--cam") forceCam = std::atoi(next()) % kNumCams;
        else if (a == "--dim") forceDim = std::clamp(std::atoi(next()), 0, 9);
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

    SDL_SetHint(SDL_HINT_APP_ID, "hyperspace");
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL: %s\n", SDL_GetError());
        return 1;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window* win = SDL_CreateWindow("Hyperspace / Ineffable", width, height,
                                       SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | (fullscreen ? SDL_WINDOW_FULLSCREEN : 0)
                                       | SDL_WINDOW_HIDDEN);  // (testing: never shown, drawn off-screen)
    setAppIcon(win);
    SDL_GLContext glctx = win ? SDL_GL_CreateContext(win) : nullptr;
    if (!win || !glctx) {
        std::fprintf(stderr, "couldn't open a window: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetWindowFullscreenMode(win, nullptr);  // NULL mode = borderless fullscreen desktop (covers the screen, no resolution switch)
    SDL_GL_SetSwapInterval(std::getenv("HYPERSPACE_NOVSYNC") || !shotPath.empty() ? 0 : 1);  // (testing: measure how fast it can go)
    if (shotPath.empty()) std::fputs(kHelp + std::strlen("hyperspace: a DMT-style audio visualizer\n"), stdout);

    GLuint vao;
    glGenVertexArrays(1, &vao);
    GLuint programs[9] = {};
    const char* sources[] = {kScene, kIneffableScene, kFx, kBright, kBlur, kPresent, kWireFragment, kLook, kCopy};
    const char* stages[] = {"Shader: scene", "Shader: mathematical worlds", "Shader: effects",
        "Shader: brightness", "Shader: blur", "Shader: display", "Shader: wireframe", "Shader: camera and effects", "Shader: copy"};
    try {
        loadWithScreen(win, glctx, [&](std::atomic<int>& stage) {
            // Draw once into a tiny target as well as linking: some drivers
            // defer their most expensive compilation until the first draw.
            Target warm; warm.make(4, 4);
            GLuint warmVao, textures[2];
            glGenVertexArrays(1, &warmVao); glBindVertexArray(warmVao);
            glGenTextures(2, textures);
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, textures[0]);
            glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,4,4,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
            glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D_ARRAY,textures[1]);
            glTexImage3D(GL_TEXTURE_2D_ARRAY,0,GL_RGBA8,4,4,48,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
            glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
            glActiveTexture(GL_TEXTURE0);
            for (int i=0;i<9;++i) {
                stage.store(i);
                programs[i]=program(sources[i], i==6?kWireVertex:kVertex);
                glUseProgram(programs[i]);
                glUniform2f(glGetUniformLocation(programs[i],"uRes"),4,4);
                glUniform1i(glGetUniformLocation(programs[i],"uHist"),1);
                glUniform1f(glGetUniformLocation(programs[i],"uSacredIntensity"),1);
                glUniform1f(glGetUniformLocation(programs[i],"uDimA"),6);
                glUniform1f(glGetUniformLocation(programs[i],"uDimB"),6);
                glUniform1f(glGetUniformLocation(programs[i],"uDimMorph"),1);
                glDrawArrays(GL_TRIANGLES,0,3);
                glFinish();
            }
            glDeleteTextures(2,textures); glDeleteVertexArrays(1,&warmVao);
            glDeleteFramebuffers(1,&warm.fbo); glDeleteTextures(1,&warm.tex);
            glBindFramebuffer(GL_FRAMEBUFFER,0);
        }, stages, 9, shotPath.empty());
    } catch (const std::exception& error) {
        std::fprintf(stderr,"Startup failed: %s\n",error.what());
        if (shotPath.empty()) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Hyperspace could not start",error.what(),win);
        SDL_Quit(); return 1;
    }
    const GLuint baseScene=programs[0], sacredScene=programs[1], fx=programs[2], bright=programs[3], blur=programs[4],
        present=programs[5], wire=programs[6], look=programs[7], copy=programs[8];
    // the settings sidebar (Dear ImGui), skipped entirely in off-screen --shot mode
    UI ui;
    const bool haveUI = shotPath.empty();
    if (haveUI) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::GetIO().IniFilename = nullptr;  // don't drop an imgui.ini in the repo
        ImGui::StyleColorsDark();
        ImGui::GetStyle().WindowRounding = 8;
        ImGui_ImplSDL3_InitForOpenGL(win, glctx);
        ImGui_ImplOpenGL3_Init("#version 330");
    }

    if (shotPath.empty()) SDL_ShowWindow(win);
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
    bool blendAll = !classic;
    const bool autoDefaults = !classic && forceDim < 0 && forceStage < 0 && mediaPath.empty();
    bool journey = false;
    float journeyTime = 0, sacredAudio = 0, sacredTime = 0;
    int dim = forceDim >= 0 ? forceDim : (journey ? 6 : 0);  // extra dimension the user wants: 0 none, 1 pillars, 2 forest, 3 corridor
    int dimA = dim, dimB = dim;               // the two worlds being blended; a change morphs the old one (A) into the new (B)
    float dimMorph = 1;                       // 0 = all A, 1 = all B (settled)
    float dimAmt = dim >= 6 ? 1.0f : 0.0f;                         // overall dimension presence (0 = none, the base trip)
    int fractal = std::getenv("HYPERSPACE_FRAC") ? std::atoi(std::getenv("HYPERSPACE_FRAC")) : 0;  // dimension 4: which fractal (0 Mandelbrot, 1 Julia, 2 Burning Ship, 3 Tricorn, 4 Multibrot); env for testing
    int frac3 = std::getenv("HYPERSPACE_FRAC3") ? std::atoi(std::getenv("HYPERSPACE_FRAC3")) : 0;  // dimension 5: 0 Mandelbulb, 1 Menger sponge, 2 Mandelbox, 3 Sierpinski, 4 quaternion Julia, 5 Apollonian
    int frac3A = frac3, frac3B = frac3;       // the 3D fractal melting from A into B
    float frac3M = 1;                         // 0 = all A, 1 = all B
    float worldZ = 0, worldSpd = 2.2f;        // how far through a dimension (pillars/forest/corridor), and the eased speed
    float f3Clock = 0;                        // the 3D fractals' own clock (its speed is a setting)
    FracDive dive;                            // dimension 4's fixed-rate zoom toward the most detail
    bool dimAuto = autoDefaults, fracAuto = autoDefaults;
    float dimWait = 12, fracWait = 20;
    uint32_t dimRng = 24681;
    auto dimRoll = [&](int n) { dimRng = dimRng * 1664525u + 1013904223u; return (int)((dimRng >> 8) % (uint32_t)n); };
    float calm = 0;                           // how much to quiet the trip's colour overlays (a dimension)
    float spread = 0.35f;                     // how wide a slice of the spectrum the palette uses: grows with the music's energy
    uint64_t last = SDL_GetTicksNS();
    int frames = 0;
    bool run = true;
    while (run) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (haveUI) ImGui_ImplSDL3_ProcessEvent(&e);
            if (e.type == SDL_EVENT_QUIT) run = false;
            if (e.type == SDL_EVENT_DROP_FILE && e.drop.data) dropped = e.drop.data;
            if (e.type != SDL_EVENT_KEY_DOWN) continue;
            if (haveUI && ImGui::GetIO().WantCaptureKeyboard) continue;  // the sidebar has the keys
            const SDL_Keycode k = e.key.key;
            if (k == SDLK_ESCAPE || k == SDLK_Q) run = false;
            else if (k == SDLK_F || k == SDLK_F11) { fullscreen = !fullscreen; SDL_SetWindowFullscreen(win, fullscreen); }
            else if (k == SDLK_TAB) ui.show = !ui.show;
            else if (k == SDLK_J) { journey = !journey; journeyTime = 0; dimAuto = false; if (journey && dim < 6) dim = 6; }
            else if (k == SDLK_N) { journey = dimAuto = false; dim = dim >= 6 ? 6 + (dim - 5) % 4 : 6; journeyTime = 0; }
            else if (k >= SDLK_1 && k <= SDLK_4) { journey = dimAuto = false; dim = 0; trip.forcedStage = (int)(k - SDLK_1); }
            else if (k == SDLK_5) { journey = dimAuto = false; dim = (dim == 5 ? 0 : 5); }  // 3D fractal
            else if (k == SDLK_6) { journey = dimAuto = false; dim = (dim == 4 ? 0 : 4); }  // fractal
            else if (k == SDLK_7) { journey = dimAuto = false; dim = (dim == 1 ? 0 : 1); }  // pillars
            else if (k == SDLK_8) { journey = dimAuto = false; dim = (dim == 2 ? 0 : 2); }  // forest
            else if (k == SDLK_9) { journey = dimAuto = false; dim = (dim == 3 ? 0 : 3); }  // corridor
            else if (k == SDLK_0) { trip.forcedStage = trip.forcedEntity = show.forcedMirror = show.forcedPattern = looks.forcedFx = looks.forcedCam = -1; dim = 0; journey = dimAuto = fracAuto = false; }
            else if (k == SDLK_V) looks.forcedFx = (looks.fx + 1) % kNumFx;
            else if (k == SDLK_C) looks.forcedCam = Looks::nextCamera(looks.forcedCam < 0 ? 0 : looks.forcedCam);
            else if (k == SDLK_E) trip.forcedEntity = (trip.entity + 1) % kNumEntities;
            else if (k == SDLK_M) show.forcedMirror = (show.mirror + 1) % kNumMirrors;
            else if (k == SDLK_P) show.forcedPattern = ((show.fading ? show.patB : show.patA) + 1) % kNumPatterns;
            else if (k == SDLK_LEFTBRACKET) scale = std::max(0.3f, scale - 0.1f);
            else if (k == SDLK_RIGHTBRACKET) scale = std::min(1.0f, scale + 0.1f);
        }
        const uint64_t now = SDL_GetTicksNS();
        const char* testStep = !shotPath.empty() ? std::getenv("HYPERSPACE_TEST_STEP") : nullptr;
        const float dt = testStep ? std::clamp((float)std::atof(testStep), 0.001f, 0.1f) : std::min((now - last) / 1e9f, 0.1f);
        last = now;
        clock += dt;

        // ---- the settings sidebar: a mandala icon toggles it; its sliders scale each effect and
        // its dropdowns pin a choice. Built now so this frame's uniforms pick up the changes.
        if (haveUI) {
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();
            ImGuiIO& io = ImGui::GetIO();
            const float W = 300.0f;

            // slide the sidebar in from the right
            ui.anim += ((ui.show ? 1.0f : 0.0f) - ui.anim) * (1.0f - std::exp(-dt / 0.12f));
            if (ui.anim > 0.002f) {
                ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - W * ui.anim, 0));
                ImGui::SetNextWindowSize(ImVec2(W, io.DisplaySize.y));
                ImGui::Begin("settings", nullptr,
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);

                auto picker = [](const char* label, int& forced, const char* const* names, int n, bool camerasOnly = false) {
                    const char* preview = forced < 0 ? "auto (the music)" : names[forced];
                    if (ImGui::BeginCombo(label, preview)) {
                        if (ImGui::Selectable("auto (the music)", forced < 0)) forced = -1;
                        for (int i = 0; i < n; ++i)
                            if ((!camerasOnly || Looks::cameraEnabled(i)) && ImGui::Selectable(names[i], forced == i)) forced = i;
                        ImGui::EndCombo();
                    }
                };

                if (ImGui::CollapsingHeader("INEFFABLE / mathematical worlds", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::Checkbox("blend all worlds and stages", &blendAll);
                    ImGui::TextDisabled("GYROID / KNOT / LOTUS / QUASICRYSTAL");
                    if (ImGui::Checkbox("continuous journey [J]", &journey)) {
                        dimAuto = false; journeyTime = 0;
                        if (journey && dim < 6) dim = 6;
                    }
                    if (ImGui::Button("next world [N]")) { journey = dimAuto = false; dim = dim >= 6 ? 6 + (dim - 5) % 4 : 6; }
                    const char* names[] = {"Gyroid cathedral", "Torus knot", "Recursive lotus", "Quasicrystal"};
                    if (dim >= 6) ImGui::Text("%s", names[dim - 6]);
                    if (journey) ImGui::ProgressBar(std::clamp(journeyTime / ui.journeyHold, 0.0f, 1.0f), ImVec2(-1, 3), "");
                    ImGui::SliderFloat("world duration", &ui.journeyHold, 12, 90, "%.0f s");
                    ImGui::SliderFloat("flow", &ui.sacredSpeed, 0.1f, 2.5f);
                    ImGui::SliderFloat("radiance", &ui.sacredIntensity, 0.2f, 2.0f);
                    ImGui::TextDisabled("J: travel / hold    N: next    0: original");
                }
                if (ImGui::CollapsingHeader("Where in the trip", ImGuiTreeNodeFlags_DefaultOpen)) {
                    const int previousStage = trip.forcedStage;
                    picker("stage", trip.forcedStage, kStageNames, 4);
                    if (trip.forcedStage != previousStage) { journey = dimAuto = false; dim = 0; }
                    picker("entity", trip.forcedEntity, kEntityNames, kNumEntities);
                    const char* const dnames[11] = {"none", "pillars", "forest", "corridor", "fractal", "3D fractal", "gyroid cathedral", "torus knot", "recursive lotus", "quasicrystal", "auto"};
                    int dimPick = dimAuto ? 10 : dim;
                    if (ImGui::Combo("dimension", &dimPick, dnames, 11)) {
                        journey = false;
                        dimAuto = dimPick == 10;
                        if (!dimAuto) dim = dimPick;
                        dimWait = dim > 0 ? 30.0f : 12.0f;
                    }
                    if (dim == 4) {
                        const char* const fnames[6] = {"Mandelbrot", "Julia", "Burning Ship", "Tricorn", "Multibrot", "auto"};
                        int fracPick = fracAuto ? 5 : fractal;
                        if (ImGui::Combo("fractal", &fracPick, fnames, 6)) {
                            fracAuto = fracPick == 5;
                            if (!fracAuto) fractal = fracPick;
                            fracWait = 20;
                        }
                    }
                    if (dim == 5) {
                        const char* const f3names[7] = {"Mandelbulb", "Menger sponge", "Mandelbox", "Sierpinski pyramid", "quaternion Julia", "Apollonian", "auto"};
                        int pick = fracAuto ? 6 : frac3;
                        if (ImGui::Combo("3D fractal", &pick, f3names, 7)) {
                            fracAuto = pick == 6;
                            if (!fracAuto) frac3 = pick;
                            fracWait = 20;
                        }
                    }
                }
                if (ImGui::CollapsingHeader("Picture", ImGuiTreeNodeFlags_DefaultOpen)) {
                    picker("pattern", show.forcedPattern, kPatternNames, kNumPatterns);
                    picker("mirror", show.forcedMirror, kMirrorNames, kNumMirrors);
                    picker("effect", looks.forcedFx, kFxNames, kNumFx);
                    picker("camera", looks.forcedCam, kCamNames, kNumCams, true);
                    ImGui::SliderFloat("sharpness", &scale, 0.3f, 1.0f, "%.2f");
                    if (ImGui::Checkbox("borderless fullscreen", &fullscreen)) SDL_SetWindowFullscreen(win, fullscreen);  // borderless desktop (mode pinned to NULL at startup)
                }
                if (ImGui::CollapsingHeader("Effects", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::SliderFloat("bloom", &ui.bloom, 0, 2);
                    ImGui::SliderFloat("trail", &ui.trail, 0, 2);
                    ImGui::SliderFloat("edge trace", &ui.trace, 0, 2);
                    ImGui::SliderFloat("contrast", &ui.contrast, 0, 2);
                    ImGui::SliderFloat("patterns", &ui.pattern, 0, 2);
                    ImGui::SliderFloat("wireframes", &ui.wires, 0, 2);
                    ImGui::SliderFloat("camera motion", &ui.camMove, 0, 2);
                    ImGui::SliderFloat("colour split", &ui.chroma, 0, 3);
                    ImGui::SliderFloat("grain", &ui.grain, 0, 3);
                }
                if (ImGui::CollapsingHeader("Colour & motion", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::SliderFloat("palette width", &ui.spread, 0, 1.5f);
                    ImGui::SliderFloat("hue drift", &ui.hueSpd, 0, 3);
                    ImGui::SliderFloat("zoom speed", &ui.zoomSpd, 0, 3);
                    ImGui::SliderFloat("spin speed", &ui.spinSpd, 0, 3);
                }
                if (ImGui::CollapsingHeader("Fractals")) {
                    ImGui::Checkbox("mirrored twin", &ui.fracTwin);
                    ImGui::Checkbox("seek the detail", &ui.fracSeek);
                    ImGui::SliderFloat("dive speed", &ui.diveSpd, 0, 3);
                    ImGui::SliderFloat("3D motion", &ui.f3Spd, 0, 3);
                    ImGui::SliderFloat("3D music bend", &ui.f3React, 0, 2);
                    ImGui::SliderFloat("3D morph time", &ui.f3Morph, 1, 20, "%.0f s");
                    ImGui::SliderFloat("auto switch every", &ui.autoHold, 5, 60, "%.0f s");
                }
                if (ImGui::CollapsingHeader("Trip events")) {
                    ImGui::Checkbox("kick ripples", &looks.ripplesOn);
                    ImGui::Checkbox("breakthroughs on drops", &looks.breaksOn);
                    {
                        const char* const bnames[3] = {"glass shatter", "white light", "torn curtain"};
                        picker("breakthrough", looks.forcedBreak, bnames, 3);
                    }
                    ImGui::SliderFloat("onset look", &ui.onset, 0, 1);
                    ImGui::SliderFloat("void grid", &ui.grid, 0, 2);
                    ImGui::SliderFloat("cloak", &ui.cloak, 0, 2);
                    ImGui::SliderFloat("watchers & eyes", &ui.watchers, 0, 1.5f);
                    ImGui::SliderFloat("entity", &ui.entity, 0, 1.5f);
                    ImGui::SliderFloat("dropped picture colours", &ui.mediaCol, 0, 1.5f);
                }
                ImGui::Dummy(ImVec2(0, 6));
                if (ImGui::Button("reset effects")) ui.resetEffects();
                ImGui::SameLine();
                if (ImGui::Button("all to auto")) {
                    trip.forcedStage = trip.forcedEntity = show.forcedMirror = show.forcedPattern = looks.forcedFx = looks.forcedCam = -1;
                    journey = false;
                    dim = 0;
                    dimAuto = fracAuto = true;
                    dimWait = 12;
                    fracWait = 20;
                }
                ImGui::End();
            }

            // the clickable fractal toggle — drawn LAST so it's always on top and never buried under
            // the panel. It rides just left of the panel's edge (so it slides out with the menu and
            // stays reachable to close it), and only shows when you hover that corner or the menu's open.
            const float R = 26.0f;
            const ImVec2 cen(io.DisplaySize.x - W * ui.anim - R - 16, io.DisplaySize.y - R - 16);
            ImGui::SetNextWindowPos(ImVec2(cen.x - R, cen.y - R));
            ImGui::SetNextWindowSize(ImVec2(R * 2, R * 2));
            ImGui::Begin("##mandala", nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings
                             | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBackground);
            ImGui::InvisibleButton("##mbtn", ImVec2(R * 2, R * 2));
            const bool hot = ImGui::IsItemHovered();
            if (ImGui::IsItemClicked()) ui.show = !ui.show;
            const float wantFade = (hot || ui.show) ? 1.0f : 0.0f;
            ui.iconFade += (wantFade - ui.iconFade) * (1.0f - std::exp(-dt / 0.15f));
            drawFractal(ImGui::GetWindowDrawList(), cen, R * 0.86f, clock, hot, ui.iconFade);
            ImGui::End();
        }

        // a picture dropped in: the trip starts over from it, its bytes setting every choice it makes
        if (!dropped.empty()) {
            if (media.load(dropped)) {
                journey = dimAuto = false; dim = 0;
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
        hue += sdt * (0.015f + 0.05f * f.mid) * ui.hueSpd;
        zoom += sdt * (0.25f + 0.9f * f.bass) * (0.3f + weight(trip.stage, 2)) * ui.zoomSpd;
        spin += sdt * (0.04f + 0.15f * f.high) * ui.spinSpd;
        // a new dimension morphs smoothly out of the one before it (ultra-psychedelic): both worlds'
        // lines crossfade. Going to/from none just fades the whole thing in/out against the base trip.
        sacredAudio += (std::min(f.bass, 2.0f) - sacredAudio) * (1.0f - std::exp(-dt / 0.45f));
        sacredTime += dt * ui.sacredSpeed * (0.85f + 0.15f * sacredAudio);
        if (journey) {
            journeyTime += dt;
            if (journeyTime >= ui.journeyHold || (journeyTime > ui.journeyHold * 0.75f && f.drop)) {
                dim = dim >= 6 ? 6 + (dim - 5) % 4 : 6;
                journeyTime = 0;
            }
        }
        if (dimAuto) {
            dimWait -= dt;
            if (dimWait <= 0) {
                dim = dim > 0 ? 0 : 1 + dimRoll(9);
                dimWait = dim > 0 ? 20.0f + dimRoll(21) : 12.0f + dimRoll(9);
            }
        }
        if (fracAuto) {
            fracWait -= dt;
            if (fracWait <= 0) {
                fractal = (fractal + 1) % 5;
                frac3 = (frac3 + 1) % 6;
                fracWait += ui.autoHold;
            }
        }
        if (dim > 0 && dim != dimB) {
            if (dimAmt < 0.02f) { dimA = dimB = dim; dimMorph = 1.0f; }  // coming from none: no morph, just fade this one in
            else { dimA = dimB; dimB = dim; dimMorph = 0.0f; }          // dimension -> dimension: morph the two together
        }
        if (const char* e = std::getenv("HYPERSPACE_DIM_AT"); e && time >= std::atof(e) && (dim == 4 || dim == 5) && dim == dimB && dimMorph >= 1.0f) dim = 9 - dim;  // testing: flip 2D <-> 3D fractal once
        if (dim > 0 && dimA + dimB == 9 && dimA * dimB == 20) dimMorph = std::min(1.0f, dimMorph + dt / ui.f3Morph);  // 2D <-> 3D fractal: a steady morph (the shader eases it)
        else if (dim > 0 && (dimA >= 6 || dimB >= 6)) dimMorph = std::min(1.0f, dimMorph + dt / 7.0f);
        else if (dim > 0) dimMorph += (1.0f - dimMorph) * (1.0f - std::exp(-dt / 1.3f));  // long, luxurious morph
        dimAmt += ((dim > 0 ? 1.0f : 0.0f) - dimAmt) * (1.0f - std::exp(-dt / 0.6f));
        const float sacredPresence = dimAmt * ((dimA >= 6 ? 1.0f : 0.0f) * (1 - dimMorph) + (dimB >= 6 ? 1.0f : 0.0f) * dimMorph);
        calm = dimAmt;  // a dimension quiets the trip's colour overlays
        float spreadT = std::clamp(0.1f + 0.28f * f.level + 0.15f * f.beat, 0.1f, 0.45f);  // a cohesive slice of the spectrum; widens a little on energy, but never the full rainbow
        spreadT = std::clamp(spreadT * ui.spread, 0.0f, 1.0f);  // the palette-width slider (1 = as above)
        spread += (spreadT - spread) * (1.0f - std::exp(-dt / 0.6f));
        for (int k = 0; k < 6; ++k) turn4[k] += sdt * (0.05f + 0.07f * k) * (0.6f + 0.6f * f.mid + 0.8f * f.beat);
        const float gridSpeed = sdt * (0.8f + 2.0f * f.bass);  // walking the grid, the way it faces
        gridX += std::sin(looks.gridTurn) * gridSpeed;
        gridZ += std::cos(looks.gridTurn) * gridSpeed;

        int ww, wh;
        SDL_GetWindowSizeInPixels(win, &ww, &wh);
        if (!shotPath.empty()) { ww = width; wh = height; }  // (testing: the off-screen size)
        if (const char* e = std::getenv("HYPERSPACE_FRAC3_AT"); e && time >= std::atof(e) && frac3 == frac3B && frac3A == frac3B) frac3 = (frac3 + 1) % 6;  // testing: switch the 3D fractal at this time
        if (frac3 != frac3B) { frac3A = frac3B; frac3B = frac3; frac3M = 0; }  // a new 3D fractal: melt into it
        frac3M = std::min(1.0f, frac3M + dt / ui.f3Morph);                    // over a few seconds (settings)
        f3Clock += dt * ui.f3Spd;
        worldSpd += (2.2f + 0.9f * f.level - worldSpd) * (1.0f - std::exp(-dt / 1.5f));  // louder = a little faster, eased
        worldZ += dt * worldSpd;
        dive.speed = ui.diveSpd; dive.seek = ui.fracSeek; dive.twin = ui.fracTwin;
        if (blendAll || dim == 4 || dimA == 4 || dimB == 4) dive.update(dt, fractal, (double)ww / std::max(1, wh), time, std::min(f.mid, 2.5f) * 0.6f);
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
        const GLuint scene = !blendAll && dimA >= 6 && dimB >= 6 && dimAmt > 0.999f && intro == 0 ? sacredScene : baseScene;
        glUseProgram(scene);
        glUniform1i(U(scene, "uBlendAll"), blendAll ? 1 : 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, sceneT[cur].tex);
        glUniform1i(U(scene, "uPrev"), 0);
        glUniform2f(U(scene, "uRes"), (float)sw, (float)sh);
        glUniform1f(U(scene, "uTime"), time);
        glUniform1f(U(scene, "uSacredTime"), sacredTime);
        glUniform1f(U(scene, "uSacredIntensity"), ui.sacredIntensity);
        glUniform1f(U(scene, "uSacredAudio"), sacredAudio);
        glUniform1f(U(scene, "uBass"), bass);
        glUniform1f(U(scene, "uMid"), mid);
        glUniform1f(U(scene, "uHigh"), high);
        glUniform1f(U(scene, "uLevel"), f.level);
        glUniform1f(U(scene, "uBeat"), f.beat);
        glUniform1f(U(scene, "uStage"), trip.stage);
        glUniform1f(U(scene, "uEntityAmt"), std::min(1.0f, trip.entityAmt * ui.entity) * (1 - (blendAll ? 0.0f : sacredPresence)));
        glUniform1i(U(scene, "uEntity"), trip.entity);
        glUniform1f(U(scene, "uMood"), f.mood);
        glUniform1f(U(scene, "uHue"), hue);
        glUniform1f(U(scene, "uSpread"), spread);
        glUniform1f(U(scene, "uZoom"), zoom);
        glUniform1f(U(scene, "uSpin"), spin);
        glUniform1f(U(scene, "uDimA"), (float)dimA);
        glUniform1f(U(scene, "uDimB"), (float)dimB);
        glUniform1f(U(scene, "uDimMorph"), dimMorph);
        glUniform1f(U(scene, "uDimAmt"), dimAmt);
        glUniform1f(U(scene, "uFractal"), (float)fractal);
        glUniform2f(U(scene, "uFracCtr"), (float)dive.cx, (float)dive.cy);
        glUniform2f(U(scene, "uJuliaC"), (float)dive.jx, (float)dive.jy);
        glUniform1f(U(scene, "uFracScale"), (float)dive.scale);
        glUniform1f(U(scene, "uFracBase"), dive.base);
        glUniform1f(U(scene, "uFracFade"), dive.fade);
        glUniform1i(U(scene, "uFracIt"), dive.iters());
        const float m3 = frac3M * frac3M * (3 - 2 * frac3M);  // eased in and out
        glUniform1f(U(scene, "uFrac3A"), (float)frac3A);
        glUniform1f(U(scene, "uFrac3B"), (float)frac3B);
        glUniform1f(U(scene, "uFrac3M"), m3);
        glUniform1f(U(scene, "uFracTwin"), ui.fracTwin ? 1.0f : 0.0f);
        glUniform1f(U(scene, "uF3Time"), f3Clock);
        glUniform1f(U(scene, "uWorldZ"), worldZ);
        glUniform1f(U(scene, "uF3React"), ui.f3React);
        glUniform1f(U(scene, "uGridAmt"), ui.grid);
        glUniform1f(U(scene, "uCalm"), calm);
        glUniform1f(U(scene, "uTrailMul"), ui.trail);
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
        glUniform1f(U(scene, "uSeedMix"), std::min(1.0f, seedMix * ui.mediaCol));
        glUniform1f(U(scene, "uGridTurn"), looks.gridTurn);
        glUniform2f(U(scene, "uGridPos"), gridX, gridZ);
        glUniform1f(U(scene, "uGridFocus"), std::clamp(trip.stage / 2.5f + 0.2f * f.level, 0.0f, 1.0f));
        glUniform1f(U(scene, "uPatAmt"), (0.35f + 0.25f * std::min(trip.stage, 2.0f) / 2 + 0.2f * f.beat) * (1 - intro) * (1 - 0.85f * calm) * (1 - sacredPresence) * ui.pattern);
        glUniform1f(U(scene, "uIntro"), intro);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        cur = nxt;

        // ---- the wireframes, added on top (they'll trail in the next frames)
        wires.clear();
        wires.aspect = (float)sw / sh;
        const float wk = (1.0f - calm) * ui.wires;  // a quieted scene dims the trip's own wireframes so what matters reads clear
        const float w0 = weight(trip.stage, 0) * wk, w1 = weight(trip.stage, 1) * wk, w2 = weight(trip.stage, 2) * wk, w3 = weight(trip.stage, 3) * wk;
        const float breathe = 1 + 0.12f * bass + 0.1f * f.beat;
        wires.metatron(0, 0, 0.17f * breathe, spin * 0.5f, palette(hue + 0.4f, f.mood, hue, spread), 0.35f * w0 + 0.7f * w1);
        static const int midShapes[3] = {14, 13, 17};  // icosahedron, dodecahedron, tesseract
        wires.polytope(midShapes[show.patA % 3], 0, 0, 0.36f * breathe, turn4, palette(hue + 0.1f, f.mood, hue, spread), 0.8f * w1);
        wires.tunnel(6 + (show.patA % 3) * 2, zoom * 0.5f, spin, hue, f.mood, 1.0f * w2);
        if (w3 > 0) {
            wires.polytope(trip.entity == kJesters ? 19 : 17, 0, 0, 0.5f * breathe, turn4, palette(hue + 0.6f, f.mood, hue, spread), 0.55f * w3, 0.005f);
            for (int i = 0; i < 6; ++i) {  // a ring of small hypercubes, turning
                const float an = spin * 0.6f + i * 1.0471976f;
                float t2[6];
                for (int k = 0; k < 6; ++k) t2[k] = turn4[k] * (1 + 0.2f * i) + i;
                wires.polytope(i % 2 ? 18 : 17, std::cos(an) * 0.72f, std::sin(an) * 0.4f, 0.08f * breathe, t2, palette(hue + i * 0.12f, f.mood, hue, spread), 0.7f * w3);
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
        glUniform1i(U(fx, "uMirror"), (intro > 0.3f || calm > 0.5f) ? 0 : show.mirror);  // no kaleidoscoping of the lines
        glUniform1f(U(fx, "uTrace"), (0.35f + 0.25f * trip.stage / 3 + 0.4f * high) * (1 - 0.8f * intro) * (1 - 0.85f * calm) * ui.trace);
        glUniform1f(U(fx, "uContrast"), (0.35f + 0.45f * trip.stage / 3) * (1 - intro) * (1 - 0.7f * calm) * ui.contrast);
        glUniform1f(U(fx, "uHue"), hue);
        glUniform1f(U(fx, "uSpread"), spread);
        glUniform1f(U(fx, "uMood"), f.mood);
        glUniform1f(U(fx, "uTurn"), spin * 0.3f);
        glUniform3fv(U(fx, "uSeedPal"), 3, &seedPal[0][0]);
        glUniform1f(U(fx, "uSeedMix"), std::min(1.0f, seedMix * ui.mediaCol));
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

        // ---- LOOK: the camera move, the picture effect, and the shatters / ripples
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
        glUniform1i(U(look, "uFx"), (intro > 0.3f || calm > 0.5f) ? 0 : looks.fx);  // no film tricks while the picture is still itself, or in a quieted scene
        {   // the camera moves (and the ones fading out); all faded down while a dropped picture is itself
            int cs[2 * Looks::kStack];
            float ts[2 * Looks::kStack], ws[2 * Looks::kStack];
            for (int i = 0; i < Looks::kStack; ++i) {
                // each slot's fading-out move goes just before its new one, where it always was in the chain,
                // so nothing jumps when a move is swapped
                cs[2 * i] = looks.outCams[i]; ts[2 * i] = looks.outTs[i]; ws[2 * i] = looks.outWs[i] * (1 - intro) * (1 - 0.8f * calm) * ui.camMove;
                cs[2 * i + 1] = looks.cams[i]; ts[2 * i + 1] = looks.camTs[i]; ws[2 * i + 1] = looks.camWs[i] * (1 - intro) * (1 - 0.8f * calm) * ui.camMove;
            }
            glUniform1iv(U(look, "uCams"), 2 * Looks::kStack, cs);
            glUniform1fv(U(look, "uCamTs"), 2 * Looks::kStack, ts);
            glUniform1fv(U(look, "uCamWs"), 2 * Looks::kStack, ws);
            glUniform1i(U(look, "uCamTest"), std::getenv("HYPERSPACE_CAMTEST") ? 1 : 0);
            glUniform1f(U(look, "uCamBeat"), looks.camBeat);
            glUniform1f(U(look, "uCamBass"), looks.camBass);
        }
        glUniform2f(U(look, "uRes"), (float)sw, (float)sh);
        glUniform1f(U(look, "uTime"), clock);
        glUniform1f(U(look, "uBass"), bass);
        glUniform1f(U(look, "uMid"), mid);
        glUniform1f(U(look, "uHigh"), high);
        glUniform1f(U(look, "uLevel"), f.level);
        glUniform1f(U(look, "uBeat"), f.beat);
        glUniform1f(U(look, "uHue"), hue);
        glUniform1f(U(look, "uSpread"), spread);
        glUniform1f(U(look, "uMood"), f.mood);
        glUniform1f(U(look, "uFxT"), looks.fxT);
        glUniform1f(U(look, "uSpeed"), looks.speed);
        glUniform1f(U(look, "uShatterT"), looks.shatterT);
        glUniform1f(U(look, "uShatterSeed"), looks.shatterSeed);
        glUniform1f(U(look, "uFigures"), looks.figures);
        glUniform1f(U(look, "uCloak"), looks.cloak * ui.cloak);
        glUniform1f(U(look, "uOnsetAmt"), ui.onset);
        glUniform1f(U(look, "uStage"), trip.stage);
        glUniform1f(U(look, "uCalm"), calm);
        glUniform1f(U(look, "uIntro"), intro);
        glUniform1f(U(look, "uBreakKind"), (float)looks.breakKind);
        glUniform1f(U(look, "uWatchers"), looks.watchers * ui.watchers);
        glUniform3fv(U(look, "uSeedPal"), 3, &seedPal[0][0]);
        glUniform1f(U(look, "uSeedMix"), std::min(1.0f, seedMix * ui.mediaCol));
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
        glUniform1f(U(present, "uBloomAmt"), (0.32f + 0.28f * trip.stage / 3 + 0.22f * f.beat) * (1 - 0.7f * intro) * (1 - 0.8f * calm) * ui.bloom);  // gentler glow so the line detail reads through it
        glUniform1f(U(present, "uCalm"), calm);
        glUniform1f(U(present, "uSpread"), spread);
        glUniform1f(U(present, "uChromaMul"), ui.chroma);
        glUniform1f(U(present, "uGrainMul"), ui.grain);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        if (const char* seq = std::getenv("HYPERSPACE_SEQ"); seq && !shotPath.empty() && clock * 30 >= seqFrame) {  // (testing: every 30th of a second, to check motion)
            glBindFramebuffer(GL_FRAMEBUFFER, shotT.fbo);
            char name[512];
            std::snprintf(name, sizeof(name), "%s/%05d.ppm", seq, seqFrame++);
            if (std::getenv("HYPERSPACE_SEQLOG")) std::printf("%d %s | beat %.2f\n", seqFrame - 1, looks.camNames().c_str(), f.beat);
            saveShot(name, ww, wh);
        }
        if (!shotPath.empty() && clock >= shotAt) {
            glBindFramebuffer(GL_FRAMEBUFFER, shotT.fbo);  // read back the finished picture (without this it sometimes came back black)
            glFinish();
            saveShot(shotPath, ww, wh);
            std::printf("saved (%s, %s %.0f%%, %s, %s, %s, %s, %.0f fps)\n", kStageNames[std::clamp((int)std::lround(trip.stage), 0, 3)],
                        kEntityNames[trip.entity], trip.entityAmt * 100, kPatternNames[show.patA], kMirrorNames[show.mirror],
                        kFxNames[looks.fx], looks.camNames().c_str(), frames / clock);
            run = false;
        }
        if (haveUI) {  // the sidebar, drawn on top of the finished picture
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
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
    if (haveUI) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
    }
    cap.stop();
    SDL_Quit();
    return 0;
}
