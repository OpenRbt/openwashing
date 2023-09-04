/*
 * menu.c
 *
 *  Created on: Feb 17, 2021
 *      Author: Roman Fedyashov
 */

#include "menu.h"

void menu_init(menu * main_menu) {
	int N1 = 3;
	main_menu->id = 1;
	main_menu->size = N1;
	main_menu->heading = STR_MAIN_MENU_CAPTION;
	main_menu->items = malloc(N1*sizeof(menu));
	init_post_number_menu(&main_menu->items[0]);
	init_mode_menu(&main_menu->items[1]);
	init_motors_menu(&main_menu->items[2]);
}

const char * str_by_num(uint8_t num) {
	switch (num) {
	    case 1:
	    	return STR_MOTORS_01;
	    case 2:
			return STR_MOTORS_02;
	    case 3:
			return STR_MOTORS_03;
	    case 4:
			return STR_MOTORS_04;
	    case 5:
			return STR_MOTORS_05;
	    case 6:
			return STR_MOTORS_06;
	    case 7:
			return STR_MOTORS_07;
	    case 8:
			return STR_MOTORS_08;
	    case 9:
			return STR_MOTORS_09;
	    case 10:
			return STR_MOTORS_10;
	    case 11:
			return STR_MOTORS_11;
	    case 12:
			return STR_MOTORS_12;

	    default:
	    	return "X";
	}
	return "XX";
}

void init_post_number_menu(menu * post_num) {
	post_num->id = 2;
	post_num->size = TOTAL_POSTS;
	post_num->heading = STR_MAIN_ITEM_POST_NUM;
	post_num->items = malloc(TOTAL_POSTS*sizeof(menu));


	for (uint8_t i=1;i<=TOTAL_POSTS;i++) {
		post_num->items[i-1].id = post_num->id * ID_OFFST + i;
		post_num->items[i-1].size = 0;
		post_num->items[i-1].heading = str_by_num(i);
		post_num->items[i-1].items = 0;
	}
}

void init_mode_menu(menu * modemenu) {
	int N3 = 2;
	modemenu->id = 3;
	modemenu->size = N3;
	modemenu->heading = STR_MAIN_ITEM_POST_MODE;
	modemenu->items = malloc(N3*sizeof(menu));

	// relay mode
	modemenu->items[0].id = modemenu->id * ID_OFFST + 1;
	modemenu->items[0].size = 0;
	modemenu->items[0].heading = STR_WORK_MODE_RELAY;
	modemenu->items[0].items = 0;

	// motor control mode
	modemenu->items[1].id = modemenu->id * ID_OFFST + 2;
	modemenu->items[1].size = 0;
	modemenu->items[1].heading = STR_WORK_MODE_MOTOR;
	modemenu->items[1].items = 0;
}

void init_motors_menu(menu * motorsmenu) {
	motorsmenu->id = ID_MOTORSMENU;
	motorsmenu->size = TOTAL_POSTS;
	motorsmenu->heading = STR_MAIN_ITEM_POST_MOTORS;
	motorsmenu->items = malloc(TOTAL_POSTS*sizeof(menu));
	for (uint8_t i=1;i<=TOTAL_POSTS;i++) {
		motorsmenu->items[i-1].id = motorsmenu->id * ID_OFFST + i;
		int N4 = 3;
		motorsmenu->items[i-1].size = N4;
		motorsmenu->items[i-1].heading = str_by_num(i);
		motorsmenu->items[i-1].items = malloc(N4*sizeof(menu));
		init_drivers_list(i-1, &motorsmenu->items[i-1]);
	}
}

void init_drivers_list(int i, menu * motorsitem) {
	// off item
	motorsitem[0].id = 0;
	motorsitem[0].size = 0;
	motorsitem[0].heading = STR_DRIVER_OFF;
	motorsitem[0].items = 0;

	// esq500 item
	motorsitem[1].id = 1;
	motorsitem[1].size = 0;
	motorsitem[1].heading = STR_DRIVER_ESQ500;
	motorsitem[1].items = 0;

	// ae200h item
	motorsitem[1].id = 2;
	motorsitem[1].size = 0;
	motorsitem[1].heading = STR_DRIVER_AE200H;
	motorsitem[1].items = 0;
}

