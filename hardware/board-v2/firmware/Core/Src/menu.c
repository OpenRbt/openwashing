/*
 * menu.c
 *
 *  Created on: Feb 17, 2021
 *      Author: Roman Fedyashov
 */

#include "menu.h"

void init_menu_item(menu_item * new_item, uint8_t new_id, const char * new_name) {
	new_item->name = new_name;
	new_item->sub_menu = 0;
	new_item->id = new_id;
}
