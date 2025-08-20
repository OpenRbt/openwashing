#pragma once
#include <string>
#include <array>

// Forward declarations to avoid pulling SDL headers into every translation unit
struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;
typedef struct _TTF_Font TTF_Font;

enum class STARTUP_MESSAGE {
    DISPLAY_INFO,     // Index 0 - appears at top
    MAC,             // Index 1
    LOCAL_IP,        // Index 2  
    SERVER_IP,       // Index 3
    POST,            // Index 4
    CARD_READER,     // Index 5
    VENDOTEK_INFO,   // Index 6
    CONFIGURATION,   // Index 7
    SETTINGS,        // Index 8
    RELAY_CONTROL_BOARD  // Index 9
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