#ifndef DIA_SCREEN_H
#define DIA_SCREEN_H
#include <stdio.h>

#define BPP 4
#define DEPTH 32

#define NO_ERROR 0
#define NULL_PASSED 1
#define IMG_ERROR 2
#define SDL_INIT_ERROR 3
#include "dia_screen.h"

DiaScreen::DiaScreen(int resX, int resY, int hideCursor, int fullScreen) {
    
    if (hideCursor) {
        SDL_ShowCursor(SDL_DISABLE);
    }
    
    // Initialize SDL2 video subsystem
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        InitializedOk = SDL_INIT_ERROR;
        return;
    }
    
    delay(100);
    
    Uint32 windowFlags = SDL_WINDOW_SHOWN;
    if (fullScreen) {
        windowFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }
    
    // Create window
    window = SDL_CreateWindow("DIA Screen",
                             SDL_WINDOWPOS_UNDEFINED,
                             SDL_WINDOWPOS_UNDEFINED,
                             resX, resY,
                             windowFlags);
    
    if (window == NULL) {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        InitializedOk = SDL_INIT_ERROR;
        return;
    }
    
    // Create renderer
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == NULL) {
        printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        // Try with software renderer as fallback
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
        if (renderer == NULL) {
            printf("Software renderer could not be created! SDL_Error: %s\n", SDL_GetError());
            InitializedOk = SDL_INIT_ERROR;
            return;
        }
    }
    
    // Create surface for compatibility with existing code
    Canvas = SDL_CreateRGBSurface(0, resX, resY, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    if (Canvas == NULL) {
        printf("Surface could not be created! SDL_Error: %s\n", SDL_GetError());
        InitializedOk = SDL_INIT_ERROR;
        return;
    }
    
    // Create texture for rendering
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, resX, resY);
    if (texture == NULL) {
        printf("Texture could not be created! SDL_Error: %s\n", SDL_GetError());
        InitializedOk = SDL_INIT_ERROR;
        return;
    }
    
    printf("SDL2 video mode is set properly \n"); 
    fflush(stdout);
    delay(100);
    InitializedOk = 1;
}

void DiaScreen_DrawPage1(DiaScreen * screen, int num) {
    printf("src1_s\n");
    printf("src2_b2\n");
    screen->FlipFrame();
    printf("src2_b3\n");
}

void DiaScreen::FlipFrame() {
    printf("frame ... ");
    
    // Update texture with surface data
    SDL_UpdateTexture(texture, NULL, Canvas->pixels, Canvas->pitch);
    
    // Clear renderer
    SDL_RenderClear(renderer);
    
    // Copy texture to renderer
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    
    // Present renderer
    SDL_RenderPresent(renderer);
    
    printf(" ... flipped \n");
}

void DiaScreen::FillBackground(Uint8 r, Uint8 g, Uint8 b) {
    SDL_FillRect(Canvas, NULL, SDL_MapRGB(Canvas->format, r, g, b));
}

DiaScreen::~DiaScreen() {
    if (texture) {
        SDL_DestroyTexture(texture);
    }
    if (Canvas) {
        SDL_FreeSurface(Canvas);
    }
    if (renderer) {
        SDL_DestroyRenderer(renderer);
    }
    if (window) {
        SDL_DestroyWindow(window);
    }
    SDL_Quit();
}

#endif