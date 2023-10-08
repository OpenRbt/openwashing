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
#define STR_TEST_RELAYS "test relays"
#define STR_EXIT "exit"
#define STR_BACK "<<< back"
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

#define MAX_MENU_LINES 6

#define MODE_SCREENSAVER 1
#define MODE_MENU 2


#include <stdint.h>
#include <stdlib.h>
#include "gm009605.h"
struct menu_s;

typedef struct menu_s {
	uint8_t id;
	const char * heading;
	uint8_t size;
	uint8_t is_selected;
	struct menu_s * items;
	struct menu_s * parent;
	void (*action)(void * arg);
	void * action_arg;

} menu;

typedef struct menu_supervisor_s {
	int8_t start_item;
	int8_t cursor;
	menu * cur_item;
	uint8_t exit;
} menu_supervisor;



void display_menu_supervisor(menu_supervisor * s);
void menu_supervisor_init(menu_supervisor * s, menu * main_menu);
void menu_init(menu * main_menu, menu_supervisor *s);
void init_post_number_menu(menu * post_num, menu_supervisor *s);
void init_mode_menu(menu * modemenu);
void init_motors_menu(menu * motorsmenu);
void init_test_menu(menu * testmenu);
void init_exit_menu(menu * exitmenu, menu_supervisor *s);
void init_drivers_list(int i, menu * motorsitem);
void display_menu_item(menu * menu_item, int8_t start_item, int8_t cursor);
void menu_supervisor_up(menu_supervisor * s);
void menu_supervisor_down(menu_supervisor * s);
void menu_supervisor_center(menu_supervisor * s);

typedef struct act_submenu_arg_s {
	menu_supervisor * s;
	menu * m;
} act_submenu_arg;
void act_submenu_pos(void *arg);

void act_test_relays(void * arg);
void act_exit(void * arg);

typedef struct act_goto_parent_arg_s {
	menu_supervisor * s;
	menu * m;
} act_goto_parent_arg;
void act_goto_parent(void * arg);

void act_set_active(void * arg);

void act_goto_menu_aux(menu_supervisor *s, menu *m, int8_t desired_position); // used to go to the specific menu

#endif /* INC_MENU_H_ */
