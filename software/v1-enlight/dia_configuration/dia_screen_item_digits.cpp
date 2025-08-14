#include "dia_screen_item_digits.h"

int DiaScreenItemDigits::Init(DiaScreenItem *base_item, json_t * item_json) {
    if (item_json == 0) {
        printf("item digits nil parameter\n");
        return 1;
    }

    json_t * position_j = json_object_get(item_json,"position");
    if (base_item->SetValue("position", position_j)) return 1;

    json_t * font_j = json_object_get(item_json,"font");
    if (base_item->SetValue("font",font_j)) return 1;

    json_t * symbol_size_j = json_object_get(item_json,"symbol_size");
    if (base_item->SetValue("symbol_size",symbol_size_j)) return 1;

    json_t * min_length_j = json_object_get(item_json,"min_length");
    if (base_item->SetValue("min_length",min_length_j)) {
        printf("digits: default min_length set to 0\n");
        base_item->SetValue("min_length", "0");
    }

    json_t * padding_j = json_object_get(item_json,"padding");
    if (base_item->SetValue("padding",padding_j)) return 1;

    json_t * length_j = json_object_get(item_json,"length");
    if (base_item->SetValue("length",length_j)) return 1;

    json_t * value_j = json_object_get(item_json,"value");
    if (base_item->SetValue("value", value_j)) {
        printf("digits: default VALUE set to 0\n");
        base_item->SetValue("value", "0");
    }

    json_t * orient_j = json_object_get(item_json,"is_vertical");
    if (base_item->SetValue("is_vertical", orient_j)) {
        printf("digits: default is_vertical set to 0\n");
        base_item->SetValue("is_vertical", "0");
    }    

    if (length.value > MAX_DIGITS) {
        printf("maximum allowed size of digits is %d, while configuration file has %d\n", MAX_DIGITS, length.value);
        return 1;
    }

    printf("Dia Screen Item: is_vertical = %d\n", is_vertical.value);

    font.InitSymbols(is_vertical.value);

    // The screen rotation is +Pi/2, or +90 degrees or counterclockwise 90 degrees;
    // i = length.value - 1 is the left part of the number, i = 0 is the right (smallest part of the numbers)
    // This code is for vertical screen
    if (is_vertical.value) {
        for (int i = 0; i < length.value; i++) {
            OutputRectangles[i] = (SDL_Rect *)malloc(sizeof(SDL_Rect));

            OutputRectangles[i]->x = position.x;
            printf("INIT: positionX %d, sizeX %d, padding %d\n", position.x, symbol_size.x, padding.value);
            OutputRectangles[i]->y = position.y + (symbol_size.y + padding.value) * i;
            OutputRectangles[i]->w = symbol_size.x;
            OutputRectangles[i]->h = symbol_size.y;
        }
    // This code is for legacy horizontal screen
    } else {
        for(int i=0;i<length.value;i++) {
            OutputRectangles[i]= (SDL_Rect *)malloc(sizeof(SDL_Rect));
            OutputRectangles[i]->x=position.x + (symbol_size.x + padding.value) * (length.value - i - 1);
            printf("INIT: positionX %d, sizeX %d, padding %d\n", position.x, symbol_size.x, padding.value);
            OutputRectangles[i]->y=position.y;
            OutputRectangles[i]->w=symbol_size.x;
            OutputRectangles[i]->h=symbol_size.y;
        }
    }
    
    if (symbol_size.x != font.SymbolSize.x || symbol_size.y != font.SymbolSize.y) {
        double xScale = (double)symbol_size.x / (double)font.SymbolSize.x;
        double yScale = (double)symbol_size.y / (double)font.SymbolSize.y;
        font.Scale(xScale, yScale, is_vertical.value);
    }
    return 0;
}

DiaIntPair DiaScreenItemDigits::getSize(){
    return this->size;
}

void DiaScreenItemDigits::SetPicture(SDL_Surface * newPicture){

}
void DiaScreenItemDigits::SetScaledPicture(SDL_Surface * newPicture){
    
}

DiaScreenItemDigits::DiaScreenItemDigits() {
    for (int i = 0; i < MAX_DIGITS; i++) {
        OutputRectangles[i] = 0;
    }
}
DiaScreenItemDigits::~DiaScreenItemDigits() {
    for (int i = 0; i < MAX_DIGITS; i++) {
        if (OutputRectangles[i] != 0) {
            free(OutputRectangles[i]);
            OutputRectangles[i] = 0;
        }
    }
}

int dia_screen_item_digits_display(DiaScreenItem * base_item, void * digits_ptr, DiaScreen * screen) {
    if (base_item == 0) {
        printf("error: nil base item\n");
        return 1;
    }
    if (digits_ptr == 0) {
        printf("error: nil digits_ptr\n");
        return 1;
    }

    DiaScreenItemDigits * digits = (DiaScreenItemDigits *)digits_ptr;

    int numberToDisplay = digits->value.value;

    SDL_Surface * fontImage = digits->font.FontImage;
    if (digits->font.ScaledFontImage != 0) {
        fontImage = digits->font.ScaledFontImage;
    }

    // Let's convert a number to an array of bytes
    char output[MAX_DIGITS];
    int remaining_value = numberToDisplay;
    int first_not_null_index = 0;
    for (int i=0;i<MAX_DIGITS;i++) {
        int curPiece = remaining_value % 10;
        remaining_value = remaining_value / 10;
        if (curPiece > 9) curPiece = 9;
        if (curPiece < 0) curPiece = 0;
        if (curPiece !=0) first_not_null_index = i;
        output[i] = curPiece;
    }

    int nums_to_output = digits->length.value;
    if (first_not_null_index + 1 < nums_to_output) nums_to_output = first_not_null_index + 1;
    if (nums_to_output<digits->min_length.value) {
        nums_to_output = digits->min_length.value;
    }

    for(int i = 0; i<nums_to_output; i++) {
        SDL_BlitSurface(fontImage,
            digits->font.SymbolRect[(int)output[i]],
            screen->Canvas,
            digits->OutputRectangles[i]);
    }

    return 0;
}

int dia_screen_item_digits_notify(DiaScreenItem * base_item, void * digits_ptr, std::string key) {
    printf("notification recieved for '%s' key\n", key.c_str());
    int error = 0;
    std::string value = base_item->GetValue(key, &error);
    if (error!=0) {
        printf("notification on non-existing key '%s'\n", key.c_str());
        return 1;
    }

    DiaScreenItemDigits *obj =  (DiaScreenItemDigits *)digits_ptr;
    if (key.compare("position")==0) {
        obj->position.Init(value);
    } else
    if (key.compare("font")==0) {
        obj->font.Init(base_item->Parent->Folder, value);
    } else
    if (key.compare("symbol_size")==0) {
        obj->symbol_size.Init(value);
    } else
    if (key.compare("min_length")==0) {
        obj->min_length.Init(value);
    } else
    if (key.compare("padding")==0) {
        obj->padding.Init(value);
    } else
    if (key.compare("length")==0) {
        obj->length.Init(value);
    } else
    if (key.compare("value")==0) {
        obj->value.Init(value);
    } else 
    if (key.compare("is_vertical")==0) {
        obj->is_vertical.Init(value);
    } else {
        printf("unknown key for digits object: '%s' \n", key.c_str());
        return 1;
    }


    return 0;
}