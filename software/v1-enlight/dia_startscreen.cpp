#include "dia_startscreen.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>

#include <array>
#include <string>
#include <iostream>
#include <cmath>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <unistd.h>

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
static bool s_vertical    = false;

// Keep old layout scaling regardless of real screen size
static int LOGICAL_W = 800;
static int LOGICAL_H = 480;

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
    if (text.empty()) return;
    
    int w=0, h=0;
    SDL_Texture* t = CreateTextTexture(text, SDL_Color{250,250,250,255}, &w, &h);
    if (!t) return;
    
    if (s_vertical) {
        // For vertical mode, we need to rotate the text 90 degrees
        // First create rotated destination rectangle
        SDL_Rect dst;
        dst.x = x;
        dst.y = LOGICAL_H - h - 60; // Position from bottom with 60px margin
        if (dst.y < 0) dst.y = 0;
        dst.w = h; // swapped for 90-degree rotation
        dst.h = w; // swapped for 90-degree rotation
        
        // Render with 90-degree rotation
        SDL_RenderCopyEx(s_renderer, t, nullptr, &dst, 90.0, nullptr, SDL_FLIP_NONE);
    } else {
        SDL_Rect dst{ x, y, w, h };
        SDL_RenderCopy(s_renderer, t, nullptr, &dst);
    }
    SDL_DestroyTexture(t);
}

static void RenderLogo() {
    if (!s_logoTex || !s_renderer) return;
    
    int w=0, h=0;
    SDL_QueryTexture(s_logoTex, nullptr, nullptr, &w, &h);
    
    // Keep original logo proportions, don't force to 120x120
    SDL_Rect dst;
    if (s_vertical) {
        // Top-left for vertical
        dst = {0, 30, w, h};
        // Render with 90-degree rotation for vertical
        SDL_RenderCopyEx(s_renderer, s_logoTex, nullptr, &dst, 90.0, nullptr, SDL_FLIP_NONE);
    } else {
        // Top-right for horizontal
        dst = {LOGICAL_W - w - 30, 30, w, h}; // Use actual logo width, not hardcoded 180
        SDL_RenderCopy(s_renderer, s_logoTex, nullptr, &dst);
    }
}

// -------- Public API --------
int StartScreenInit(std::string path) {
    // Check for vertical orientation flag
    if (path.back() == '/') {
        path = path.substr(0, path.size()-1);
    }
    std::string verticalFlag = path + "/_vertical";
    FILE* flag = fopen(verticalFlag.c_str(), "r");
    if (flag) {
        s_vertical = true;
        fclose(flag);
    }

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
            0, 0, // Let SDL choose the size
            SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_SHOWN
        );
        if (!s_window) {
            std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
            return -3;
        }
        s_ownWindow = true;
    }

    // Now get the actual window size after creation
    SDL_GetWindowSize(s_window, &s_resX, &s_resY);
    
    // Use actual screen size, no logical scaling
    LOGICAL_W = s_resX;
    LOGICAL_H = s_resY;

    if (!s_renderer) {
        s_renderer = SDL_CreateRenderer(s_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!s_renderer) {
            std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << std::endl;
            return -4;
        }
        SDL_SetRenderDrawBlendMode(s_renderer, SDL_BLENDMODE_BLEND);
        s_ownRenderer = true;
    }

    // Don't set logical size - use native resolution for fullscreen
    // SDL_RenderSetLogicalSize(s_renderer, LOGICAL_W, LOGICAL_H);

    // Calculate font size to match original SDL1.2 appearance
    // Original: line_size = info->current_h / (MAX_MESSAGES * 2) = height / 20
    int fontPx = s_resY / 20; // Keep original calculation for proper size
    if (fontPx < 24) fontPx = 24; // Ensure minimum readable size
    if (fontPx > 72) fontPx = 72; // Cap for very large screens

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

    // Optional: load a logo
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

    // Set initial display info message to match original behavior
    std::string orientStr = s_vertical ? "VERTICAL" : "HORIZONTAL";
    StartScreenMessage(STARTUP_MESSAGE::DISPLAY_INFO, 
        "Display: " + std::to_string(s_resX) + "x" + std::to_string(s_resY) + " | " + orientStr);

    // First frame
    StartScreenUpdate();
    return 0;
}

void StartScreenDrawBase() {
    if (!s_renderer) return;
    SDL_SetRenderDrawColor(s_renderer, 0, 0, 0, 255);
    SDL_RenderClear(s_renderer);
    RenderLogo();
}

void StartScreenDrawMessages() {
    if (!s_renderer || !s_font) return;

    const int lineHeight = TTF_FontHeight(s_font);
    
    if (s_vertical) {
        // Vertical mode: each message has fixed column position
        for (size_t i = 0; i < s_messages.size(); ++i) {
            if (!s_messages[i].empty()) {
                int x = lineHeight + (lineHeight * i); // Original formula
                RenderTextLine(s_messages[i], x, 0); // y will be adjusted in RenderTextLine
            }
        }
    } else {
        // Horizontal mode: start from very top
        for (size_t i = 0; i < s_messages.size(); ++i) {
            if (!s_messages[i].empty()) {
                int y = 10 + (lineHeight * i); // Start at 10px from very top
                RenderTextLine(s_messages[i], 60, y);
            }
        }
    }
}

void StartScreenDrawVertical()  { StartScreenDrawBase(); StartScreenDrawMessages(); }
void StartScreenDrawHorizontal(){ StartScreenDrawBase(); StartScreenDrawMessages(); }

void StartScreenUpdate() {
    if (!s_renderer) return;
    if (s_vertical) {
        StartScreenDrawVertical();
    } else {
        StartScreenDrawHorizontal();
    }
    SDL_RenderPresent(s_renderer);
}

void StartScreenUpdateIP() {
    // Get local IP address using same logic as original
    int sock = socket(PF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in loopback;

    if (sock == -1) {
        StartScreenMessage(STARTUP_MESSAGE::LOCAL_IP, "Local IP: ");
        return;
    }

    memset(&loopback, 0, sizeof(loopback));
    loopback.sin_family = AF_INET;
    loopback.sin_addr.s_addr = INADDR_LOOPBACK;   
    loopback.sin_port = htons(9);                 

    if (connect(sock, reinterpret_cast<struct sockaddr*>(&loopback), sizeof(loopback)) == -1) {
        close(sock);
        StartScreenMessage(STARTUP_MESSAGE::LOCAL_IP, "Local IP: ");
        return;
    }

    socklen_t addrlen = sizeof(loopback);
    if (getsockname(sock, reinterpret_cast<struct sockaddr*>(&loopback), &addrlen) == -1) {
        close(sock);
        StartScreenMessage(STARTUP_MESSAGE::LOCAL_IP, "Local IP: ");
        return;
    }

    close(sock);

    char buf[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &loopback.sin_addr, buf, INET_ADDRSTRLEN) == 0x0) {    
        StartScreenMessage(STARTUP_MESSAGE::LOCAL_IP, "Local IP: ");
        return;
    } 

    std::string baseIP(buf);
    StartScreenMessage(STARTUP_MESSAGE::LOCAL_IP, "Local IP: " + baseIP);
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

    // No SDL_Quit() here – app owns global SDL lifecycle.
}