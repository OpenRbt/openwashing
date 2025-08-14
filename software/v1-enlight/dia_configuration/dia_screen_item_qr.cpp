#include "dia_screen_item_qr.h"
#include "dia_functions.h"

DiaScreenItemQr::DiaScreenItemQr() {
}

DiaScreenItemQr::~DiaScreenItemQr() {
}

int DiaScreenItemQr::Init(DiaScreenItem *base_item, json_t * item_json) {
    if (item_json == 0) {
        printf("item qr nil parameter\n");
        return 1;
    }
    printf("qr init started \n");


    json_t * position_j = json_object_get(item_json,"position");
    
    if (base_item->SetValue("position", position_j)) return 1;

    json_t * size_j = json_object_get(item_json,"size");
    if (base_item->SetValue("size",size_j)) return 1;

    json_t * src_j = json_object_get(item_json,"src");
    if (base_item->SetValue("src",src_j)) return 1;

    json_t * url_j = json_object_get(item_json,"url");
    if (base_item->SetValue("url", url_j)) return 1;

    Rescale();
    return 0;
}

int DiaScreenItemQr::SetQr(std::string url) {
    const QrCode::Ecc errCorLvl = QrCode::Ecc::HIGHERR;  // Error correction level
    const QrCode qr = QrCode::encodeText(url.c_str(), errCorLvl);
    SDL_Surface *qrSurface = dia_QRToSurface(qr);

    SDL_Surface * scaledSurface = dia_ScaleSurface(qrSurface, this->getSize().x, this->getSize().y);
    
    // SDL2: Convert surface to appropriate format
    SDL_Surface *scaledQR = SDL_ConvertSurfaceFormat(scaledSurface, SDL_PIXELFORMAT_ARGB8888, 0);
    if (!scaledQR) {
        printf("error: SDL_ConvertSurfaceFormat: %s\n", SDL_GetError());
        scaledQR = scaledSurface; // Fallback to original surface
    } else {
        SDL_FreeSurface(scaledSurface);
    }
    
    this->SetScaledPicture(scaledQR);

    if (qrSurface!=0) {
        SDL_FreeSurface(qrSurface);
    }

    return 1;
}

DiaString DiaScreenItemQr::getUrl(){
    return this->url;
}

int dia_screen_item_qr_display(DiaScreenItem * base_item, void * image_ptr, DiaScreen * screen) {
    if (base_item == 0) {
        printf("error: nil base item\n");
        return 1;
    }

    if (image_ptr == 0) {
        printf("error: nil image_ptr\n");
        return 1;
    }

    DiaScreenItemQr * myQr = (DiaScreenItemQr *)image_ptr;
    myQr->SetQr(myQr->getUrl().value);

    SDL_Surface * curPict = myQr->Picture;

    if (myQr->ScaledPicture) {
        curPict = myQr->ScaledPicture;
        printf("scaled qr used\n");
    } else {
        printf("original qr used \n");
    }

    SDL_BlitSurface(curPict,
                NULL,
                screen->Canvas,
                myQr->OutputRectangle);

    printf("qr '%s' displayed at (%d, %d) size (%d, %d) \n", myQr->src.value.c_str(),
    myQr->OutputRectangle->x,
    myQr->OutputRectangle->y,
    myQr->OutputRectangle->w,
    myQr->OutputRectangle->h);

    return 0;
}

int dia_screen_item_qr_notify(DiaScreenItem * base_item, void * image_ptr, std::string key) {
    int error = 0;
    std::string value = base_item->GetValue(key, &error);
    if (error!=0) {
        printf("notification on non-existing key '%s'\n", key.c_str());
        return 1;
    }

    DiaScreenItemQr *obj =  (DiaScreenItemQr *)image_ptr;
    if (key.compare("position")==0) {
        obj->position.Init(value);
        obj->OutputRectangle->x = obj->position.x;
        obj->OutputRectangle->y = obj->position.y;
    } else
    if (key.compare("size")==0) {
        obj->size.Init(value);
        obj->OutputRectangle->w = obj->size.x;
        obj->OutputRectangle->h = obj->size.y;
        obj->Rescale();
    } else
    if (key.compare("click_id") == 0) {
        obj->click_id.Init(value);
    } else 
    if (key.compare("url") == 0) {
        obj->url.Init(value);
        obj->SetQr(value);
    } else
    if (key.compare("src") == 0) {
        obj->src.Init(value);
        std::string full_name = base_item->Parent->Folder;
        full_name+="/";
        full_name+=obj->src.value;
        SDL_Surface *tmpImg = IMG_Load(full_name.c_str());

        if(!tmpImg) {
            printf("error: IMG_Load: %s\n", IMG_GetError());
            printf("%s error\n", full_name.c_str());
            return 1;
        }
        
        // SDL2: Convert surface to appropriate format
        SDL_Surface *newImg = SDL_ConvertSurfaceFormat(tmpImg, SDL_PIXELFORMAT_ARGB8888, 0);
        if (!newImg) {
            printf("error: SDL_ConvertSurfaceFormat: %s\n", SDL_GetError());
            newImg = tmpImg; // Fallback to original surface
        } else {
            SDL_FreeSurface(tmpImg);
        }
        
        obj->SetPicture(newImg);
        obj->Rescale();
	} else {
        printf("unknown key for qr object: '%s' \n", key.c_str());
        return 1;
    }

    return 0;
}