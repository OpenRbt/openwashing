#ifndef _DIA_SCREEN_ITEM_DIGITS_H
#define _DIA_SCREEN_ITEM_DIGITS_H
#include <jansson.h>
#include <string>
#include "dia_screen_item.h"
#include "dia_all_items.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#define MAX_DIGITS 9

class DiaScreenItemDigits : public SpecificObjectPtr {
public:
    DiaIntPair position;
    DiaIntPair size;
    DiaFont font;
    DiaIntPair symbol_size;
    DiaNumber padding;
    DiaNumber min_length;
    DiaNumber length;
    DiaNumber value;
    DiaNumber is_vertical;

    SDL_Rect * OutputRectangles[MAX_DIGITS];

    virtual DiaIntPair getSize();
    virtual void SetPicture(SDL_Surface * newPicture);
    virtual void SetScaledPicture(SDL_Surface * newPicture);

    int Init(DiaScreenItem * base_item,json_t * item_json);
    virtual ~DiaScreenItemDigits();
    DiaScreenItemDigits();
};
#endif // _DIA_SCREEN_ITEM_DIGITS_H

int dia_screen_item_digits_display(DiaScreenItem * base_item, void * digits_ptr, DiaScreen * screen);
int dia_screen_item_digits_notify(DiaScreenItem * base_item, void * digits_ptr, std::string key);