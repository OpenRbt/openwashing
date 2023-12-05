#ifndef _DIA_SCREEN_ITEM_QR_H
#define _DIA_SCREEN_ITEM_QR_H
#include <jansson.h>
#include <string>
#include <iostream>
#include <dia_string.h>
#include <dia_functions.h>
#include "dia_screen_item_image.h"

class DiaScreenItemQr : public DiaScreenItemImage {
public:
    DiaString url;

    int Init(DiaScreenItem * base_item, json_t * item_json);
    int SetQr(std::string url);
    DiaString getUrl();
    virtual ~DiaScreenItemQr();
    DiaScreenItemQr();
};
#endif // _DIA_SCREEN_ITEM_QR_H

int dia_screen_item_qr_display(DiaScreenItem * base_item, void * image_ptr, DiaScreen * screen);
int dia_screen_item_qr_notify(DiaScreenItem * base_item, void * image_ptr, std::string key);