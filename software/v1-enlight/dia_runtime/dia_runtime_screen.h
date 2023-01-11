#ifndef dia_runtime_screen_h
#define dia_runtime_screen_h

#include "dia_functions.h"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

#include "../QR/qrcodegen.hpp"
#include "../QR/EasyBMP.hpp"
#include "LuaBridge.h"
#include "dia_screen_item_image.h"
#include <string>
#include <jansson.h>
#include <list>

using namespace luabridge;
using std::uint8_t;
using qrcodegen::QrCode;
using qrcodegen::QrSegment;
using EasyBMP::Image;
using EasyBMP::RGBColor;

class DiaRuntimeScreen {
public:
    std::string Name;
    void * object;
    
    int (*set_value_function)(void * object, const char *element, const char * key, const char * value);

    int SetValue(const char * key, const char * value) {
        //printf("[%s]->[%s] = [%s];\n", Name.c_str(), key, value);
        if(object!=0 && set_value_function!=0) {
            char * buf = strdup(key);
            char * sub_key = buf;
            for(char * i=buf; i[0]!=0; i++) {
                if (*i=='.') {
                    *i=0;
                    sub_key = i+1;
                    break;
                }
            }
            set_value_function(object, buf , sub_key, value);
            free(buf);
        } else {
            printf("error: NIL object or function\n");
        }

        return 0;
    }

    int (*set_QR_function)(SDL_Surface * qrSurface);

    void GenerateQR(std::string address){

        const char *text = address.c_str();              // User-supplied text
        const QrCode::Ecc errCorLvl = QrCode::Ecc::HIGHERR;  // Error correction level
        const QrCode qr = QrCode::encodeText(text, errCorLvl);

        RGBColor white(255, 255, 255);  
        Image img(qr.getSize(), qr.getSize(), "samples/wash/pic/qr.bmp", white);

        SDL_Surface * qrSurface = dia_QRToSurface(qr);

        set_QR_function(qrSurface);
        /*
        std::map<std::string, DiaScreenConfig *>::iterator it;
        for (it=config->ScreenConfigs.begin(); it!=config->ScreenConfigs.end(); it++) {
            it->second->SetQr(qrSurface);
        }
        */
        //Перенести код в новую функцию, которая будет олбъявлена в диа_фирмваре. Объявить анонимную функцию и передать сюда
    }

    

    std::string GetValue(std::string key) {
        return "hello";
    }

    
    void * screen_object;
    int (*display_screen) (void * screen_object, void * screen_config);
    int Display() {
        if(display_screen) {
            display_screen(screen_object, object);
        }
        return 0;
    }

    DiaRuntimeScreen() {
        object = 0;
        screen_object = 0;
        set_value_function = 0;
        display_screen = 0;
    }
};

#endif
