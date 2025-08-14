#include "dia_startscreen.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>

#include <array>
#include <string>
#include <iostream>
#include <cmath>

// -------- Module state --------
static SDL_Window*   s_window   = nullptr;
static SDL_Renderer* s_renderer = nullptr;
static SDL_Texture*  s_logoTex  = nullptr;
static TTF_Font*     s_font     = nullptr;

static int  s_resX = 800;     // desktop (physical) width
static int  s_resY = 480;     // desktop (physical) height
static bool s_ownWindow   = false;
static bool s_ownRenderer = false;
static bool s_ownTTF      = false;
static bool s_ownIMG      = false;

// Keep old layout scaling regardless of real screen size
static const int LOGICAL_W = 800;
static const int LOGICAL_H = 480;

static std::array<std::string, 10> s_messages{};

// -------- Helpers --------
static TTF_Font* OpenFontAtPixelHeight(const std::string& path, int pxTarget) {
    float ddpi=96.f, hdpi=96.f, vdpi=96.f;
    if (SDL_GetDisplayDPI(0, &ddpi, &hdpi, &vdpi) != 0) {
        hdpi = vdpi = 96.f; // fallback
    }
    const int pt = (int)std::lround(pxTarget * 72.0 / std::max(1.0f, vdpi));

#if SDL_TTF_VERSION_ATLEAST(2,20,0)
    return TTF_OpenFontDPI(path.c_str(), pt, (int)hdpi, (int)vdpi);
#else
    return TTF_OpenFont(path.c_str(), pt);
#endif
}

static SDL_Texture* CreateTextTexture(const std::string& text, SDL_Color color, int* outW=nullptr, int* outH=nullptr) {
    if (!s_font || !s_renderer || text.empty()) return nullptr;
    SDL_Surface* surf = TTF_RenderUTF8_Blended(s_font, text.c_str(), color);
    if (!surf) {
        std::cerr << "TTF_RenderUTF8_Blended failed: " << TTF_GetError() << std::endl;
        return nullptr;
    }
    SDL_Texture* tex = SDL_CreateTextureFromSurface(s_renderer, surf);
    if (!tex) {
        std::cerr << "SDL_CreateTextureFromSurface failed: " << SDL_GetError() << std::endl;
        SDL_FreeSurface(surf);
        return nullptr;
    }
    if (outW) *outW = surf->w;
    if (outH) *outH = surf->h;
    SDL_FreeSurface(surf);
    return tex;
}

static void RenderTextLine(const std::string& text, int x, int y) {
    int w=0, h=0;
    SDL_Texture* t = CreateTextTexture(text, SDL_Color{255,255,255,255}, &w, &h);
    if (!t) return;
    SDL_Rect dst{ x, y, w, h };
    SDL_RenderCopy(s_renderer, t, nullptr, &dst);
    SDL_DestroyTexture(t);
}

static void RenderLogoTopRight(int rightMargin = 16, int topMargin = 16) {
    if (!s_logoTex || !s_renderer) return;
    int w=0, h=0;
    SDL_QueryTexture(s_logoTex, nullptr, nullptr, &w, &h);
    SDL_Rect dst{ LOGICAL_W - w - rightMargin, topMargin, w, h };
    SDL_RenderCopy(s_renderer, s_logoTex, nullptr, &dst);
}

// -------- Public API --------
int StartScreenInit(std::string path) {
    // This module does NOT own global SDL lifecycle (no SDL_Quit here)

    // Init TTF if needed
    if (!TTF_WasInit()) {
        if (TTF_Init() != 0) {
            std::cerr << "TTF_Init failed: " << TTF_GetError() << std::endl;
            return -2;
        }
        s_ownTTF = true;
    }

    // Init IMG (PNG)
    int want = IMG_INIT_PNG;
    if ((IMG_Init(0) & want) != want) {
        int got = IMG_Init(want);
        if ((got & want) != want) {
            std::cerr << "IMG_Init PNG failed: " << IMG_GetError() << std::endl;
        } else {
            s_ownIMG = true;
        }
    }

    // Get desktop display size
    SDL_DisplayMode dm{};
    if (SDL_GetDesktopDisplayMode(0, &dm) == 0 && dm.w > 0 && dm.h > 0) {
        s_resX = dm.w;
        s_resY = dm.h;
    }

    // Reuse existing window/renderer if app already made them
    if (!s_window) {
        s_window = SDL_GL_GetCurrentWindow(); // may be null
    }
    if (!s_renderer && s_window) {
        s_renderer = SDL_GetRenderer(s_window);
    }

    // Create fullscreen window if needed
    if (!s_window) {
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1"); // linear scaling
        s_window = SDL_CreateWindow(
            "Start Screen",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            s_resX, s_resY,
            SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_SHOWN
        );
        if (!s_window) {
            std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
            return -3;
        }
        s_ownWindow = true;
    }

    if (!s_renderer) {
        s_renderer = SDL_CreateRenderer(s_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!s_renderer) {
            std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << std::endl;
            return -4;
        }
        SDL_SetRenderDrawBlendMode(s_renderer, SDL_BLENDMODE_BLEND);
        s_ownRenderer = true;
    }

    // Keep “old look”: fixed logical canvas; (0,0) is the top-left corner
    SDL_RenderSetLogicalSize(s_renderer, LOGICAL_W, LOGICAL_H);

    // Load font to match previous on-screen size (~18pt@96DPI ≈ 24px)
    const int fontPx = 24;
    const std::string p1 = path + "/Roboto-Regular.ttf";
    const std::string p2 = path + "/fonts/Roboto-Regular.ttf";
    const std::string p3 = path + "/font.ttf";

    s_font = OpenFontAtPixelHeight(p1, fontPx);
    if (!s_font) s_font = OpenFontAtPixelHeight(p2, fontPx);
    if (!s_font) s_font = OpenFontAtPixelHeight(p3, fontPx);
    if (!s_font) {
        std::cerr << "No usable .ttf found in: " << path
                  << " (tried Roboto-Regular.ttf, fonts/Roboto-Regular.ttf, font.ttf)"
                  << std::endl;
        return -5;
    }

    // Optional: load a logo (top-right, doesn’t affect top-left text)
    const std::string logoPath = path + "/logo.png";
    if (SDL_RWops* rw = SDL_RWFromFile(logoPath.c_str(), "rb")) {
        SDL_Surface* logoSurf = IMG_LoadPNG_RW(rw);
        SDL_RWclose(rw);
        if (logoSurf) {
            s_logoTex = SDL_CreateTextureFromSurface(s_renderer, logoSurf);
            if (!s_logoTex) {
                std::cerr << "SDL_CreateTextureFromSurface(logo) failed: " << SDL_GetError() << std::endl;
            }
            SDL_FreeSurface(logoSurf);
        }
    }

    // First frame
    StartScreenUpdate();
    return 0;
}

void StartScreenDrawBase() {
    if (!s_renderer) return;
    SDL_SetRenderDrawColor(s_renderer, 0, 0, 0, 255);
    SDL_RenderClear(s_renderer);
    RenderLogoTopRight();
}

void StartScreenDrawMessages() {
    if (!s_renderer) return;

    // EXACT top-left start
    int x = 0;
    int y = 0;

    // Use actual font height for steady line spacing
    const int lh = s_font ? TTF_FontHeight(s_font) : 24;

    for (const auto& line : s_messages) {
        if (!line.empty()) {
            RenderTextLine(line, x, y);
            y += lh;
        }
    }
}

void StartScreenDrawVertical()  { StartScreenDrawBase(); StartScreenDrawMessages(); }
void StartScreenDrawHorizontal(){ StartScreenDrawBase(); StartScreenDrawMessages(); }

void StartScreenUpdate() {
    if (!s_renderer) return;
    StartScreenDrawBase();
    StartScreenDrawMessages();
    SDL_RenderPresent(s_renderer);
}

void StartScreenUpdateIP() {
    // implement if needed; then call StartScreenMessage(...)
}

void StartScreenMessage(STARTUP_MESSAGE type, std::string msg) {
    const size_t idx = static_cast<size_t>(type);
    if (idx < s_messages.size()) {
        s_messages[idx] = std::move(msg);
        StartScreenUpdate();
    }
}

void StartScreenShutdown() {
    // Close font BEFORE TTF_Quit() to avoid use-after-free
    if (s_font) {
        TTF_CloseFont(s_font);
        s_font = nullptr;
    }
    if (s_logoTex) {
        SDL_DestroyTexture(s_logoTex);
        s_logoTex = nullptr;
    }

    // Only destroy what we created
    if (s_ownRenderer && s_renderer) {
        SDL_DestroyRenderer(s_renderer);
        s_renderer = nullptr;
        s_ownRenderer = false;
    }
    if (s_ownWindow && s_window) {
        SDL_DestroyWindow(s_window);
        s_window = nullptr;
        s_ownWindow = false;
    }

    if (s_ownTTF && TTF_WasInit()) {
        TTF_Quit();
        s_ownTTF = false;
    }
    if (s_ownIMG) {
        IMG_Quit();
        s_ownIMG = false;
    }

    // No SDL_Quit() here — app owns global SDL lifecycle.
}
