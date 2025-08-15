#include "dia_startscreen.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>

#include <array>
#include <string>
#include <iostream>
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

static int  s_resX = 1920;
static int  s_resY = 1080;
static int  s_logoWidth = 0;
static int  s_logoHeight = 0;
static bool s_ownWindow   = false;
static bool s_ownRenderer = false;
static bool s_ownTTF      = false;
static bool s_ownIMG      = false;
static bool s_vertical    = false;

static std::array<std::string, 10> s_messages{};

static void RenderTextLineHorizontal(const std::string& text, int x, int y) {
    if (text.empty() || !s_font || !s_renderer) return;
    
    SDL_Surface* surf = TTF_RenderUTF8_Blended(s_font, text.c_str(), SDL_Color{250, 250, 250, 255});
    if (!surf) return;
    
    SDL_Texture* tex = SDL_CreateTextureFromSurface(s_renderer, surf);
    if (tex) {
        SDL_Rect dst = {x, y, surf->w, surf->h};
        SDL_RenderCopy(s_renderer, tex, nullptr, &dst);
        SDL_DestroyTexture(tex);
    }
    SDL_FreeSurface(surf);
}

static void RenderTextLineVertical(const std::string& text, int x, int y) {
    if (text.empty() || !s_font || !s_renderer) return;
    
    SDL_Surface* surf = TTF_RenderUTF8_Blended(s_font, text.c_str(), SDL_Color{250, 250, 250, 255});
    if (!surf) return;
    
    SDL_Texture* tex = SDL_CreateTextureFromSurface(s_renderer, surf);

    if (tex) {
        SDL_Rect dst = {x, y, surf->w, surf->h};
        SDL_Point center = {0, 0};
        SDL_RenderCopyEx(s_renderer, tex, nullptr, &dst, -90.0, &center, SDL_FLIP_NONE);
        SDL_DestroyTexture(tex);
    }
    SDL_FreeSurface(surf);
}

static void RenderLogoHorizontal() {
    if (!s_logoTex || !s_renderer || s_logoWidth == 0 || s_logoHeight == 0) return;
    
    // Horizontal mode: logo at top-right
    SDL_Rect dst = {s_resX - s_logoWidth - 60, 30, s_logoWidth, s_logoHeight};
    SDL_RenderCopy(s_renderer, s_logoTex, nullptr, &dst);
}

static void RenderLogoVertical() {
    if (!s_logoTex || !s_renderer || s_logoWidth == 0 || s_logoHeight == 0) return;
    
    // Vertical mode: logo at what would be top-left in horizontal (now left side)
    SDL_Rect dst = {30, 0, s_logoWidth, s_logoHeight};
    
    // Rotate logo 90 degrees counterclockwise for vertical mode
    SDL_Point center = {s_logoWidth / 2, s_logoHeight / 2}; // Center of the actual logo
    SDL_RenderCopyEx(s_renderer, s_logoTex, nullptr, &dst, -90.0, &center, SDL_FLIP_NONE);
}

static void StartScreenDrawMessagesHorizontal() {
    if (!s_renderer || !s_font) return;

    const int lineHeight = TTF_FontHeight(s_font);
    
    for (size_t i = 0; i < s_messages.size(); ++i) {
        if (!s_messages[i].empty()) {
            int y = 10 + (lineHeight * i);
            RenderTextLineHorizontal(s_messages[i], 60, y);
        }
    }
}

static void StartScreenDrawMessagesVertical() {
   if (!s_renderer || !s_font) return;

   const int lineHeight = TTF_FontHeight(s_font);
   
   for (size_t i = 0; i < s_messages.size(); ++i) {
       if (!s_messages[i].empty()) {
           int x = 10 + (lineHeight * i);
           RenderTextLineVertical(s_messages[i], x, s_resY - 60);
       }
   }
}

// -------- Public API --------
int StartScreenInit(std::string path) {
    // Check for vertical orientation flag
    if (!path.empty() && path.back() == '/') {
        path = path.substr(0, path.size()-1);
    }
    std::string verticalFlag = path + "/_vertical";
    FILE* flag = fopen(verticalFlag.c_str(), "r");
    if (flag) {
        s_vertical = true;
        fclose(flag);
    }

    // Init SDL if needed
    if (!SDL_WasInit(SDL_INIT_VIDEO)) {
        if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
            std::cerr << "SDL_InitSubSystem failed: " << SDL_GetError() << std::endl;
            return -1;
        }
    }

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
    int got = IMG_Init(want);
    if ((got & want) != want) {
        std::cerr << "IMG_Init PNG failed: " << IMG_GetError() << std::endl;
        // Continue anyway, might still work
    } else {
        s_ownIMG = true;
    }

    // Reuse existing window/renderer if app already made them
    if (!s_window) {
        s_window = SDL_GL_GetCurrentWindow();
    }
    if (!s_renderer && s_window) {
        s_renderer = SDL_GetRenderer(s_window);
    }

    // Create fullscreen window if needed
    if (!s_window) {
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
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

    // Get actual window size after creation
    SDL_GetWindowSize(s_window, &s_resX, &s_resY);

    if (!s_renderer) {
        s_renderer = SDL_CreateRenderer(s_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!s_renderer) {
            std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << std::endl;
            return -4;
        }
        SDL_SetRenderDrawBlendMode(s_renderer, SDL_BLENDMODE_BLEND);
        s_ownRenderer = true;
    }

    // Calculate font size: use original formula but cap it reasonably
    int fontPx = s_resY / 20; // Original formula from SDL1.2
    if (fontPx < 24) fontPx = 24; // Minimum readable size
    if (fontPx > 60) fontPx = 60; // Maximum for very large screens

    // Try to load font
    const std::string p1 = path + "/Roboto-Regular.ttf";
    const std::string p2 = path + "/fonts/Roboto-Regular.ttf";
    const std::string p3 = path + "/font.ttf";

    s_font = TTF_OpenFont(p1.c_str(), fontPx);
    if (!s_font) s_font = TTF_OpenFont(p2.c_str(), fontPx);
    if (!s_font) s_font = TTF_OpenFont(p3.c_str(), fontPx);
    if (!s_font) {
        std::cerr << "No usable .ttf found in: " << path << std::endl;
        return -5;
    }

    // Try to load logo
    const std::string logoPath = path + "/logo.png";
    if (SDL_RWops* rw = SDL_RWFromFile(logoPath.c_str(), "rb")) {
        SDL_Surface* logoSurf = IMG_LoadPNG_RW(rw);
        SDL_RWclose(rw);
        if (logoSurf) {
            // Store the actual logo dimensions
            s_logoWidth = logoSurf->w;
            s_logoHeight = logoSurf->h;
            s_logoTex = SDL_CreateTextureFromSurface(s_renderer, logoSurf);
            SDL_FreeSurface(logoSurf);
        }
    }

    // Set initial display info message
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
    
    // Use appropriate logo rendering function
    if (s_vertical) {
        RenderLogoVertical();
    } else {
        RenderLogoHorizontal();
    }
}

void StartScreenDrawMessages() {
    if (s_vertical) {
        StartScreenDrawMessagesVertical();
    } else {
        StartScreenDrawMessagesHorizontal();
    }
}

void StartScreenDrawVertical() { 
    StartScreenDrawBase(); 
    StartScreenDrawMessagesVertical(); 
}

void StartScreenDrawHorizontal() { 
    StartScreenDrawBase(); 
    StartScreenDrawMessagesHorizontal(); 
}

void StartScreenUpdate() {
    if (!s_renderer) return;
    StartScreenDrawBase();
    StartScreenDrawMessages();
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
    if (s_font) {
        TTF_CloseFont(s_font);
        s_font = nullptr;
    }
    if (s_logoTex) {
        SDL_DestroyTexture(s_logoTex);
        s_logoTex = nullptr;
        s_logoWidth = 0;
        s_logoHeight = 0;
    }

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
}