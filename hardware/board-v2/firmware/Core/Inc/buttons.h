/*
 * buttons.h
 *
 *  Created on: Jan 28, 2021
 *      Author: Roman Fedyashov
 */

#ifndef INC_BUTTONS_H_
#define INC_BUTTONS_H_
#include "main.h"
#include <stdint.h>
#define BUTTONS_COUNT 3
#define BOUNCE_SAMPLES 3
// Please make sure dbl click fits in dbl_click_countdown var
#define DBL_CLICK_CYCLES 100;
typedef struct button_s {
  uint16_t pin;
  GPIO_TypeDef * port;
  int8_t bounce_analog_state;
  uint8_t digital_state;
  uint8_t is_clicked;
  uint8_t is_dblclicked;
  uint16_t dbl_click_countdown;
} button;

typedef struct buttons_t {
  button btn[BUTTONS_COUNT];
  uint8_t cursor;
} buttons;

void add_button(buttons * obj, uint16_t pin, GPIO_TypeDef * port);
void init_buttons(buttons * obj);
void update_button(button * obj);

#endif /* INC_BUTTONS_H_ */
