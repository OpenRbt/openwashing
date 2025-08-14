#include "dia_startscreen.h"
#define DEPTH 32
#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_image.h>

#include <stdio.h>

#include <unistd.h>
#include <sstream>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>

#include "resources/roboto_regular_packed.h"
#include "resources/logo_packed.h"

#include "3rd/SDL_gfx/SDL_rotozoom.h"

#define MAX_MESSAGES 10

SDL_Surface *surface;
SDL_Surface *logo;
TTF_Font *font;
SDL_Color bgColor = {0, 0, 0};
SDL_Color txtColor = {250, 250, 250};
std::string messages[MAX_MESSAGES];
int line_size = 90;
int logo_x = 1740;

bool vertical = false;
int resX;
int resY;

int StartScreenInit(std::string path) {
    FILE *flag;
    if (path[path.length()-1] == '/'){
        path = path.substr(0, path.size()-1);
    }
    if ((flag = fopen((path + "/_vertical").c_str(), "r"))){
        vertical = true;
        fclose(flag);
    }
    
    // SDL is already initialized in main(), so we don't call SDL_Init() here
    const SDL_VideoInfo *info = SDL_GetVideoInfo();
    surface = SDL_SetVideoMode(info->current_w, info->current_h, DEPTH, SDL_NOFRAME | SDL_HWSURFACE);
    if (!surface) {
        fprintf(stderr, "Failed to init startup screen\n");
        return -1;
    }
    TTF_Init();
    logo = IMG_Load_RW(SDL_RWFromMem((void *)LOGO, LOGO_SIZE),1);
    line_size = info->current_h / (MAX_MESSAGES * 2);
    if (vertical){
        logo_x = 0;
    } else {
        logo_x = info->current_w - 180;
    }
    
    resX = info->current_w;
    resY = info->current_h;
    font = TTF_OpenFontRW(SDL_RWFromConstMem(ROBOTO_REGULAR, ROBOTO_REGULAR_SIZE),1, line_size);
    StartScreenMessage(STARTUP_MESSAGE::DISPLAY_INFO, "Display: " + std::to_string(info->current_w) + "x" + std::to_string(info->current_h) + " | " + (vertical ? "VERTICAL" : "HORIZONTAL"));
    return 0;
}

void StartScreenDrawVertical(){
    SDL_FillRect(surface, NULL, SDL_MapRGB(surface->format, bgColor.r, bgColor.g, bgColor.b));
    //logoImage
    SDL_Rect rect = {(short int)logo_x, 30, 120, 120};
    SDL_Surface *rotatedLogo = rotozoomSurface(logo, vertical ? 90 : 0, 1.0 , 0);
    SDL_BlitSurface(rotatedLogo, NULL, surface, &rect);
    SDL_FreeSurface(rotatedLogo);

    for (int i = 0; i < MAX_MESSAGES; i++) {
        SDL_Rect rect;
        SDL_Surface *message = TTF_RenderText_Solid(font, messages[i].c_str(), txtColor);
        SDL_Surface *rotated = rotozoomSurface(message, vertical ? 90 : 0, 1.0 , 0);
        
        int h = 0;
        if (rotated) {
            h = rotated->h;
        }
        short int offset = resY - h- 60;
        if (offset < 0){
            offset = 0;
        }

        rect = {(short int)(line_size + line_size * i), offset, 0, 0};            
              
        SDL_BlitSurface(rotated, NULL, surface, &rect);
        SDL_FreeSurface(rotated);
        SDL_FreeSurface(message);
    }
}

void StartScreenDrawHorizontal(){
    SDL_FillRect(surface, NULL, SDL_MapRGB(surface->format, bgColor.r, bgColor.g, bgColor.b));
    //logoImage
    SDL_Rect rect = {(short int)logo_x, 30, 120, 120};
    SDL_Surface *rotatedLogo = rotozoomSurface(logo, vertical ? 90 : 0, 1.0 , 0);
    SDL_BlitSurface(rotatedLogo, NULL, surface, &rect);
    SDL_FreeSurface(rotatedLogo);

    for (int i = 0; i < MAX_MESSAGES; i++) {
        SDL_Rect rect;
        SDL_Surface *message = TTF_RenderText_Solid(font, messages[i].c_str(), txtColor);
        SDL_Surface *rotated = rotozoomSurface(message, vertical ? 90 : 0, 1.0 , 0);
        
        rect = {60, (short int)(line_size + line_size * i), 0, 0};
              
        SDL_BlitSurface(rotated, NULL, surface, &rect);
        SDL_FreeSurface(rotated);
        SDL_FreeSurface(message);
    }
}

void StartScreenUpdate() {
    if (vertical){
        StartScreenDrawVertical();
    } else {
        StartScreenDrawHorizontal();
    }
    SDL_Flip(surface);
}

void StartScreenUpdateIP(){
    int sock = socket(PF_INET, SOCK_DGRAM, 0);
    sockaddr_in loopback;

    if (sock == -1) {
        StartScreenMessage(STARTUP_MESSAGE::LOCAL_IP, "Local IP: ");
        return;
    }

    memset(&loopback, 0, sizeof(loopback));
    loopback.sin_family = AF_INET;
    loopback.sin_addr.s_addr = INADDR_LOOPBACK;   
    loopback.sin_port = htons(9);                 

    if (connect(sock, reinterpret_cast<sockaddr*>(&loopback), sizeof(loopback)) == -1) {
        close(sock);
        StartScreenMessage(STARTUP_MESSAGE::LOCAL_IP, "Local IP: ");
        return;
    }

    socklen_t addrlen = sizeof(loopback);
    if (getsockname(sock, reinterpret_cast<sockaddr*>(&loopback), &addrlen) == -1) {
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
	printf("Base IP: %s\n", baseIP.c_str());
}

void StartScreenShutdown() {
    TTF_Quit();
    if (surface) {
        SDL_FreeSurface(surface);
    }
    if (logo) {
        SDL_FreeSurface(logo);
    }
    // Don't call SDL_Quit() here - let main() handle it
}

void StartScreenMessage(STARTUP_MESSAGE type, std::string msg) {
    switch (type) {
    case STARTUP_MESSAGE::DISPLAY_INFO:
        messages[0] = msg;
        break;
    case STARTUP_MESSAGE::MAC:
        messages[1] = msg;
        break;
    case STARTUP_MESSAGE::LOCAL_IP:
        messages[2] = msg;
        break;
    case STARTUP_MESSAGE::SERVER_IP:
        messages[3] = msg;
        break;
    case STARTUP_MESSAGE::POST:
        messages[4] = msg;
        break;
    case STARTUP_MESSAGE::CARD_READER:
        messages[5] = msg;
        break;
    case STARTUP_MESSAGE::VENDOTEK_INFO:
        messages[6] = msg;
        break;
    case STARTUP_MESSAGE::CONFIGURATION:
        messages[7] = msg;
        break;
    case STARTUP_MESSAGE::SETTINGS:
        messages[8] = msg;
        break;
    case STARTUP_MESSAGE::RELAY_CONTROL_BOARD:
        messages[9] = msg;
        break;
    default:
        break;
    }
    StartScreenUpdate();
}