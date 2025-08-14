#pragma once
#include <string>
#include <array>

// Forward declarations to avoid pulling SDL headers into every translation unit
struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;
typedef struct _TTF_Font TTF_Font;

enum class STARTUP_MESSAGE {
    MAC,
    POST,
    LOCAL_IP,
    SERVER_IP,
    CARD_READER,
    VENDOTEK_INFO,
    CONFIGURATION,
    SETTINGS,
    RELAY_CONTROL_BOARD,
    DISPLAY_INFO
};

// Initializes the start screen module.
// - path: directory that contains assets (e.g., "logo.png", a .ttf font).
// Returns 0 on success, non-zero on failure.
int StartScreenInit(std::string path);

// Draw helpers (safe to no-op if not used)
void StartScreenDrawBase();
void StartScreenDrawMessages();
void StartScreenDrawVertical();
void StartScreenDrawHorizontal();

// Present current frame
void StartScreenUpdate();

// Optionally refresh IP string if you have such logic elsewhere
void StartScreenUpdateIP();

// Set/replace a message and immediately refresh the screen
void StartScreenMessage(STARTUP_MESSAGE type, std::string msg);

// Proper SDL2 teardown for this module (closes font/texture BEFORE TTF_Quit)
void StartScreenShutdown();
