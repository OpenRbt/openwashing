#include "dia_screen_item_image.h"
#include "dia_functions.h"

int DiaScreenItemImage::Init(DiaScreenItem *base_item, json_t * item_json) {
    if (item_json == 0) {
        printf("item image nil parameter\n");
        return 1;
    }
    printf("image init started \n");

    json_t * position_j = json_object_get(item_json,"position");
    if (base_item->SetValue("position", position_j)) return 1;

    json_t * size_j = json_object_get(item_json,"size");
    if (base_item->SetValue("size",size_j)) return 1;

    json_t * src_j = json_object_get(item_json,"src");
    if (base_item->SetValue("src",src_j)) return 1;

    json_t * click_id_j = json_object_get(item_json,"click_id");
    if (base_item->SetValue("click_id", click_id_j)) {
        printf("digits: default CLICK ID set to 0\n");
        base_item->SetValue("click_id", "0");
    }

    Rescale();
    return 0;
}

void DiaScreenItemImage::Rescale() {
    if(Picture!=0) {
        int x = Picture->w;
        int y = Picture->h;
        if (x!=size.x || y!= size.y) {
            SDL_Surface * surfaceCur = dia_ScaleSurface(Picture, size.x, size.y);
            SetScaledPicture(surfaceCur);
        }
    }
}

DiaScreenItemImage::DiaScreenItemImage() {
    Picture = 0;
    ScaledPicture = 0;
    OutputRectangle = (SDL_Rect *)malloc(sizeof(SDL_Rect));
}

DiaIntPair DiaScreenItemImage::getSize(){
    return this->size;
}

void DiaScreenItemImage::SetPicture(SDL_Surface * newPicture) {
    if (Picture!=0) {
        SDL_FreeSurface(Picture);
        Picture = 0;
    }
    
    Picture = newPicture;
}

void DiaScreenItemImage::SetScaledPicture(SDL_Surface * newPicture) {
    if (ScaledPicture!=0) {
        SDL_FreeSurface(ScaledPicture);
        ScaledPicture = 0;
    }
    ScaledPicture = newPicture;
}

DiaScreenItemImage::~DiaScreenItemImage() {
    if (Picture!=0) {
        printf("\nSDL_FreeSurface(Picture);\n");
        SDL_FreeSurface(Picture);
        Picture = 0;
    }
    if (ScaledPicture!=0) {
        printf("\nSDL_FreeSurface(ScaledPicture);\n");
        SDL_FreeSurface(ScaledPicture);
        ScaledPicture = 0;
    }
    if(OutputRectangle!=0) {
        printf("\nfree(OutputRectangle);\n");
        free(OutputRectangle);
        OutputRectangle = 0;
    }
}

int dia_screen_item_image_display(DiaScreenItem * base_item, void * image_ptr, DiaScreen * screen) {
    if (base_item == 0) {
        printf("error: nil base item\n");
        return 1;
    }

    if (image_ptr == 0) {
        printf("error: nil image_ptr\n");
        return 1;
    }

    DiaScreenItemImage * myImg = (DiaScreenItemImage *)image_ptr;

    SDL_Surface * curPict = myImg->Picture;

    if (myImg->ScaledPicture) {
        curPict = myImg->ScaledPicture;
        printf("scaled picture used\n");
    } else {
        printf("original img used \n");
    }

    SDL_BlitSurface(curPict,
                NULL,
                screen->Canvas,
                myImg->OutputRectangle);

    // printf("img '%s' displayed at (%d, %d) size (%d, %d) \n", myImg->src.value.c_str(),
    // myImg->OutputRectangle->x,
    // myImg->OutputRectangle->y,
    // myImg->OutputRectangle->w,
    // myImg->OutputRectangle->h);

    return 0;
}

int dia_screen_item_image_notify(DiaScreenItem * base_item, void * image_ptr, std::string key) {
    int error = 0;
    std::string value = base_item->GetValue(key, &error);
    if (error!=0) {
        printf("notification on non-existing key '%s'\n", key.c_str());
        return 1;
    }

    DiaScreenItemImage *obj =  (DiaScreenItemImage *)image_ptr;
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
        printf("unknown key for image object: '%s' \n", key.c_str());
        return 1;
    }

    return 0;
}