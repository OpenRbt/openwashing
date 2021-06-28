/*
 * menu.h
 *
 *  Created on: Feb 17, 2021
 *      Author: Roman Fedyashov
 */

#ifndef INC_MENU_H_
#define INC_MENU_H_

#include <stdint.h>
struct menu_item_s;
struct menu_s;


typedef struct menu_item_s {
	uint8_t id;
	const char * name;
	struct menu_s * sub_menu;
} menu_item;

void init_menu_item(menu_item * new_item, uint8_t new_id, const char * new_name);

typedef struct menu_s {
	const char * heading;
	uint8_t size;
	struct menu_item_s items[10];
} menu;

#endif /* INC_MENU_H_ */
