#ifndef _DIA_SCREEN_ITEM_IMAGE_ARRAY_H
#define _DIA_SCREEN_ITEM_IMAGE_ARRAY_H
#include <jansson.h>
#include <string>
#include "dia_screen_item.h"
#include "dia_all_items.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#define MAX_PICTURES 32

class DiaScreenItemImageArray : public SpecificObjectPtr  {
public:
    DiaIntPair position;
    DiaIntPair size;
    DiaNumber length;
    DiaNumber index;

    SDL_Rect * OutputRectangle;
    SDL_Surface * Pictures[MAX_PICTURES];
    SDL_Surface * ScaledPictures[MAX_PICTURES];

    int appendPos;

    int Init(DiaScreenItem * base_item, json_t * item_json);
    
    virtual DiaIntPair getSize();
    virtual void SetPicture(SDL_Surface * newPicture);
    virtual void SetScaledPicture(SDL_Surface * newPicture);

    void AppendPicture(SDL_Surface * newPicture);
    //void Rescale();

    ~DiaScreenItemImageArray();
    DiaScreenItemImageArray();
};
#endif // _DIA_SCREEN_ITEM_IMAGE_ARRAY_H

int dia_screen_item_image_array_display(DiaScreenItem * base_item, void * image_array_ptr, DiaScreen * screen);
int dia_screen_item_image_array_notify(DiaScreenItem * base_item, void * image_array_ptr, std::string key);