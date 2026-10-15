// A dropped picture, GIF or video: the seed of a trip. ffmpeg (already on the system) decodes it in the
// background to 640x360 frames (with alpha), 30 a second (videos and GIFs loop; their sound is ignored,
// the music still comes from the speakers). From the picture come the trip's colours (its three main
// ones), and from the file's bytes a number, so the same file always takes the same trip.
// A thing on a plain background (a pill on white, a logo) or a see-through picture is CUT OUT: shown
// whole in the middle, the background left to the trip, the colours taken from the thing itself.
#pragma once

#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// another program, run without a shell (so odd file names are safe), and what it prints
class Pipe {
public:
    ~Pipe() { close(); }
    bool open(const std::vector<std::string>& args)
    {
        std::vector<char*> av;  // made before forking: the child shouldn't allocate
        for (const auto& a : args) av.push_back(const_cast<char*>(a.c_str()));
        av.push_back(nullptr);
        int p[2];
        if (pipe(p)) return false;
        fPid = fork();
        if (fPid == 0) {
            dup2(p[1], 1);
            const int nul = ::open("/dev/null", O_RDWR);
            dup2(nul, 0);
            dup2(nul, 2);
            ::close(p[0]);
            ::close(p[1]);
            execvp(av[0], av.data());
            _exit(127);
        }
        ::close(p[1]);
        fFd = p[0];
        return fPid > 0;
    }
    // exactly n bytes; false at the end
    bool read(void* buf, size_t n)
    {
        char* b = static_cast<char*>(buf);
        while (n) {
            const ssize_t r = ::read(fFd, b, n);
            if (r <= 0) return false;
            b += r;
            n -= (size_t)r;
        }
        return true;
    }
    std::string readAll()
    {
        std::string s;
        char b[256];
        ssize_t r;
        while ((r = ::read(fFd, b, sizeof(b))) > 0) s.append(b, (size_t)r);
        return s;
    }
    void close()
    {
        if (fFd >= 0) ::close(fFd);
        if (fPid > 0) {
            kill(fPid, SIGTERM);
            waitpid(fPid, nullptr, 0);
        }
        fFd = fPid = -1;
    }

private:
    pid_t fPid = -1;
    int fFd = -1;
};

class Media {
public:
    static constexpr int kW = 640, kH = 360;
    bool hasPicture = false;
    bool cutout = false;  // its background removed (see above)
    uint32_t seed = 0;
    std::string name;

    ~Media() { stop(); }

    // false if ffmpeg can't find a picture in it
    bool load(const std::string& path)
    {
        if (!has(path, "v")) return false;
        stop();
        hasPicture = true;
        seed = hashFile(path);
        name = path.substr(path.find_last_of('/') + 1);
        look(path);
        fPalReady = false;
        fPath = path;
        fRun = true;
        fThread = std::thread([this] { loop(); });
        return true;
    }
    void stop()
    {
        fRun = false;
        if (fThread.joinable()) fThread.join();
    }
    // the newest frame, if there's been a new one since last time
    bool frame(std::vector<uint8_t>& out)
    {
        std::lock_guard<std::mutex> g(fLock);
        if (!fNew) return false;
        out = fFrame;
        fNew = false;
        return true;
    }
    // its three main colours (0..1), once the first frame's been looked at
    bool palette(float out[3][3])
    {
        if (!fPalReady) return false;
        std::lock_guard<std::mutex> g(fLock);
        std::copy(&fPal[0][0], &fPal[0][0] + 9, &out[0][0]);
        return true;
    }

private:
    static bool has(const std::string& path, const char* kind)
    {
        Pipe p;
        if (!p.open({"ffprobe", "-v", "error", "-select_streams", kind, "-show_entries", "stream=codec_type", "-of", "csv=p=0", path})) return false;
        return !p.readAll().empty();
    }
    // FNV-1a over the size and the first megabyte
    static uint32_t hashFile(const std::string& path)
    {
        uint32_t h = 2166136261u;
        if (FILE* f = std::fopen(path.c_str(), "rb")) {
            std::vector<unsigned char> b(1 << 20);
            const size_t n = std::fread(b.data(), 1, b.size(), f);
            std::fseek(f, 0, SEEK_END);
            const long size = std::ftell(f);
            std::fclose(f);
            for (size_t i = 0; i < n; ++i) h = (h ^ b[i]) * 16777619u;
            h = (h ^ (uint32_t)size) * 16777619u;
        }
        return h;
    }

    void loop()
    {
        std::vector<uint8_t> buf((size_t)kW * kH * 4);
        const std::string vf = cutout ? "format=rgba,scale=600:340:force_original_aspect_ratio=decrease,pad=640:360:-1:-1:color=0x00000000,fps=30"
                                      : "format=rgba,scale=640:360:force_original_aspect_ratio=increase,crop=640:360,fps=30";
        auto next = std::chrono::steady_clock::now();
        while (fRun) {
            Pipe p;
            if (!p.open({"ffmpeg", "-v", "error", "-nostdin", "-i", fPath, "-an", "-vf", vf,
                         "-f", "rawvideo", "-pix_fmt", "rgba", "-"})) return;
            int frames = 0;
            while (fRun && p.read(buf.data(), buf.size())) {
                if (fKey) key(buf);
                {
                    std::lock_guard<std::mutex> g(fLock);
                    fFrame = buf;
                    fNew = true;
                }
                if (!fPalReady) findPalette(buf);
                ++frames;
                next += std::chrono::microseconds(33333);
                std::this_thread::sleep_until(next);
            }
            if (frames <= 1) return;  // a still picture: that one frame is all there is
            next = std::chrono::steady_clock::now();  // a video or GIF: round again
        }
    }

    // a small first look: see-through, or a plain background all round the edge? then cut it out
    void look(const std::string& path)
    {
        cutout = fKey = false;
        const int n = 96;
        std::vector<uint8_t> px((size_t)n * n * 4);
        Pipe p;
        if (!p.open({"ffmpeg", "-v", "error", "-nostdin", "-i", path, "-frames:v", "1", "-vf", "format=rgba,scale=96:96", "-f", "rawvideo", "-pix_fmt", "rgba", "-"}) ||
            !p.read(px.data(), px.size()))
            return;
        int clear = 0;
        double sum[3] = {}, sq[3] = {};
        int edge = 0;
        for (int y = 0; y < n; ++y)
            for (int x = 0; x < n; ++x) {
                const uint8_t* q = &px[((size_t)y * n + x) * 4];
                if (q[3] < 200) ++clear;
                if (x > 3 && x < n - 4 && y > 3 && y < n - 4) continue;  // the outer ring
                for (int c = 0; c < 3; ++c) { sum[c] += q[c] / 255.0; sq[c] += (q[c] / 255.0) * (q[c] / 255.0); }
                ++edge;
            }
        double spread = 0;
        for (int c = 0; c < 3; ++c) {
            fBg[c] = (float)(sum[c] / edge);
            spread += sq[c] / edge - fBg[c] * fBg[c];
        }
        if (clear > n * n / 50) cutout = true;              // it's see-through somewhere: use that
        else if (std::sqrt(std::max(spread, 0.0)) < 0.05) cutout = fKey = true;  // one flat colour all round: key it out
    }
    // take out everything close to the background colour (softly, so edges stay smooth)
    void key(std::vector<uint8_t>& px) const
    {
        for (size_t i = 0; i < px.size(); i += 4) {
            float d = 0;
            for (int c = 0; c < 3; ++c) { const float e = px[i + c] / 255.0f - fBg[c]; d += e * e; }
            const float a = std::clamp((std::sqrt(d) - 0.06f) / 0.1f, 0.0f, 1.0f);
            px[i + 3] = (uint8_t)std::min<float>(px[i + 3], a * 255);
        }
    }

    // k-means on a sample of the pixels: the three colours most of the picture is made of, most first
    void findPalette(const std::vector<uint8_t>& px)
    {
        std::vector<float> s;
        for (int y = 5; y < kH; y += 10)
            for (int x = 5; x < kW; x += 10)
                if (px[((size_t)y * kW + x) * 4 + 3] > 128)  // only what's really there, not the cut-out background
                    for (int c = 0; c < 3; ++c) s.push_back(px[((size_t)y * kW + x) * 4 + c] / 255.0f);
        const int n = (int)s.size() / 3;
        if (n < 3) return;
        std::vector<int> order(n);
        for (int i = 0; i < n; ++i) order[i] = i;
        auto luma = [&](int i) { return s[i * 3] * 0.3f + s[i * 3 + 1] * 0.59f + s[i * 3 + 2] * 0.11f; };
        std::sort(order.begin(), order.end(), [&](int a, int b) { return luma(a) < luma(b); });
        float c[3][3];
        for (int k = 0; k < 3; ++k)  // start from a dark, a middling and a bright pixel
            for (int ch = 0; ch < 3; ++ch) c[k][ch] = s[order[(n - 1) * (1 + 2 * k) / 6] * 3 + ch];
        int count[3] = {0, 0, 0};
        for (int it = 0; it < 8; ++it) {
            float sum[3][3] = {};
            count[0] = count[1] = count[2] = 0;
            for (int i = 0; i < n; ++i) {
                int best = 0;
                float bd = 1e9f;
                for (int k = 0; k < 3; ++k) {
                    float d = 0;
                    for (int ch = 0; ch < 3; ++ch) d += (s[i * 3 + ch] - c[k][ch]) * (s[i * 3 + ch] - c[k][ch]);
                    if (d < bd) { bd = d; best = k; }
                }
                ++count[best];
                for (int ch = 0; ch < 3; ++ch) sum[best][ch] += s[i * 3 + ch];
            }
            for (int k = 0; k < 3; ++k)
                if (count[k])
                    for (int ch = 0; ch < 3; ++ch) c[k][ch] = sum[k][ch] / count[k];
        }
        int rank[3] = {0, 1, 2};
        std::sort(rank, rank + 3, [&](int a, int b) { return count[a] > count[b]; });
        std::lock_guard<std::mutex> g(fLock);
        for (int k = 0; k < 3; ++k)
            for (int ch = 0; ch < 3; ++ch) fPal[k][ch] = c[rank[k]][ch];
        fPalReady = true;
    }

    std::string fPath;
    std::thread fThread;
    std::atomic<bool> fRun{false}, fPalReady{false};
    std::mutex fLock;
    std::vector<uint8_t> fFrame;
    bool fNew = false;
    float fPal[3][3] = {};
    bool fKey = false;  // a flat background colour to take out
    float fBg[3] = {};
};
