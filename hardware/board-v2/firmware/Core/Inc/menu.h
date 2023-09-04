/*
 * menu.h
 *
 *  Created on: Feb 17, 2021
 *      Author: Roman Fedyashov
 */

#ifndef INC_MENU_H_
#define INC_MENU_H_
#define STR_MAIN_MENU_CAPTION "main menu"
#define STR_MAIN_ITEM_POST_NUM "position"
#define STR_MAIN_ITEM_POST_MODE "mode"
#define STR_WORK_MODE_RELAY "control post"
#define STR_WORK_MODE_MOTOR "motors driver"
#define STR_MAIN_ITEM_POST_MOTORS "motors"
#define TOTAL_POSTS 12
#define STR_MOTORS_01 "01"
#define STR_MOTORS_02 "02"
#define STR_MOTORS_03 "03"
#define STR_MOTORS_04 "04"
#define STR_MOTORS_05 "05"
#define STR_MOTORS_06 "06"
#define STR_MOTORS_07 "07"
#define STR_MOTORS_08 "08"
#define STR_MOTORS_09 "09"
#define STR_MOTORS_10 "10"
#define STR_MOTORS_11 "11"
#define STR_MOTORS_12 "12"

#define STR_DRIVER_OFF "off"
#define STR_DRIVER_ESQ500 "esq500"
#define STR_DRIVER_AE200H "ae200h"

#define ID_OFFST 16

// IDS OF SETTINGS TO BE STORED
#define ID_MOTORSMENU 4

#include <stdint.h>
#include <stdlib.h>
struct menu_s;

typedef struct menu_s {
	uint8_t id;
	const char * heading;
	uint8_t size;
	struct menu_s * items;
} menu;

void menu_init(menu * main_menu);
void init_post_number_menu(menu * post_num);
void init_mode_menu(menu * modemenu);
void init_motors_menu(menu * motorsmenu);
void init_drivers_list(int i, menu * motorsitem);

#endif /* INC_MENU_H_ */
