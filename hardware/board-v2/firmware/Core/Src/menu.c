/*
 * menu.c
 *
 *  Created on: Feb 17, 2021
 *      Author: Roman Fedyashov
 */

#include "menu.h"
#include "app.h"

void menu_supervisor_init(menu_supervisor * s, menu * main_menu){
	s->cur_item = main_menu;
	s->start_item = 0;
	s->cursor = 0;
	s->exit = 0;
}

void display_menu_supervisor(menu_supervisor * s) {
	display_menu_item(s->cur_item, s->start_item, s->cursor);
}

void menu_supervisor_up(menu_supervisor * s) {
	if (s->cursor <= 0) { // If its already the first element
		if (s->cur_item->size < MAX_MENU_LINES) {
			s->start_item = 0;
		} else {
			s->start_item =  1 + s->cur_item->size - MAX_MENU_LINES;;
		}
		s->cursor = s->cur_item->size - 1;
		return;
	}
	int8_t curLine = s->cursor - s->start_item;
	if (curLine <=0) { // if we're already on the first line
		s->cursor--;
		s->start_item--;
		return;
	}
	s->cursor--;
}

void menu_supervisor_down(menu_supervisor * s) {
	if (s->cursor >= s->cur_item->size-1) { // If its already the latest element
		s->start_item = 0;
		s->cursor = 0;
		return;
	}
	int8_t curLine = s->cursor - s->start_item + 1;
	if (curLine >= MAX_MENU_LINES - 1) { // if we're already on the last line
		s->cursor++;
		s->start_item++;
		return;
	}
	s->cursor++;
}

void menu_supervisor_center(menu_supervisor * s) {
	menu * selected_item = &(s->cur_item->items[s->cursor]);

	if (selected_item->action != 0) {
		void * arg = selected_item->action_arg;
		selected_item->action(arg);
	}
}

void menu_init(menu * main_menu, menu_supervisor *s) {
	int N1 = 5;
	main_menu->id = 1;
	main_menu->size = N1;
	main_menu->heading = STR_MAIN_MENU_CAPTION;
	main_menu->parent = 0;
	main_menu->action = 0;
	main_menu->action_arg = 0;

	main_menu->items = malloc(N1*sizeof(menu));
	// Let's set up a parent for all
	for (int8_t i=0;i<N1;i++) {
		main_menu->items[i].parent = main_menu;
	}

	init_post_number_menu(&main_menu->items[0], s);
	init_mode_menu(&main_menu->items[1], s);
	init_motors_menu(&main_menu->items[2], s);
	init_test_menu(&main_menu->items[3]);
	init_exit_menu(&main_menu->items[4], s);
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

void init_test_menu(menu * test_menu) {
	test_menu->id = 6;
	test_menu->size = 0;
	test_menu->is_selected = 0;
	test_menu->items = 0;
	test_menu->heading = STR_TEST_RELAYS;
	test_menu->action = act_test_relays;
	test_menu->action_arg = 0;
}

void init_exit_menu(menu * exit_menu, menu_supervisor *s) {
	exit_menu->id = 7;
	exit_menu->size = 0;
	exit_menu->items = 0;
	exit_menu->heading = STR_EXIT;
	exit_menu->action = act_exit;
	exit_menu->action_arg = s;
	exit_menu->is_selected = 0;
}

void init_post_number_menu(menu * post_num, menu_supervisor *s) {
	post_num->id = 2;
	post_num->size = TOTAL_POSTS + 1;
	post_num->heading = STR_MAIN_ITEM_POST_NUM;
	post_num->items = malloc((TOTAL_POSTS + 1)*sizeof(menu));
	post_num->is_selected = 0;
	act_submenu_arg * args = malloc(sizeof(act_submenu_arg)); // will never be destroyed

	args->m = post_num;
	args->s = s;

	post_num->action = act_submenu_pos;
	post_num->action_arg = args;

	for (uint8_t i=1;i<=TOTAL_POSTS;i++) {
		post_num->items[i-1].id = i;
		post_num->items[i-1].size = 0;
		post_num->items[i-1].heading = str_by_num(i);
		post_num->items[i-1].items = 0;
		post_num->items[i-1].action = act_set_active;
		post_num->items[i-1].action_arg = &(post_num->items[i-1]);
		post_num->items[i-1].parent = post_num;
	}

	// exit item
	post_num->items[TOTAL_POSTS].id = 0; // TO DO remove id field
	post_num->items[TOTAL_POSTS].size = 0;
	post_num->items[TOTAL_POSTS].heading = STR_BACK;
	post_num->items[TOTAL_POSTS].items = 0;
	post_num->items[TOTAL_POSTS].action = act_goto_parent;
	act_goto_parent_arg * post_args_back = malloc(sizeof(act_goto_parent_arg)); // never deleted
	post_args_back->m = post_num;
	post_args_back->s = s;
	post_num->items[TOTAL_POSTS].action_arg = post_args_back;
}

void init_mode_menu(menu * modemenu, menu_supervisor *s) {
	int N3 = 3;
	modemenu->id = 3;
	modemenu->size = N3;
	modemenu->heading = STR_MAIN_ITEM_POST_MODE;
	modemenu->items = malloc(N3*sizeof(menu));
	modemenu->action = 0;
	modemenu->is_selected = 0;
	act_submenu_arg * args = malloc(sizeof(act_submenu_arg)); // will never be destroyed

	args->m = modemenu;
	args->s = s;

	modemenu->action = act_submenu_mode;
	modemenu->action_arg = args;

	int i=0;

	// rs485
	modemenu->items[i].id = i;
	modemenu->items[i].size = 0;
	modemenu->items[i].heading = STR_MODE_RS485;
	modemenu->items[i].items = 0;
	modemenu->items[i].action = 0;
	modemenu->items[i].is_selected = 0;
	modemenu->items[i].items = 0;
	modemenu->items[i].action = act_set_active_mode;
	modemenu->items[i].action_arg = &(modemenu->items[0]);
	modemenu->items[i].parent = modemenu;

	// usb
	i++;
	modemenu->items[i].id = i;
	modemenu->items[i].size = 0;
	modemenu->items[i].heading = STR_MODE_USB;
	modemenu->items[i].items = 0;
	modemenu->items[i].action = 0;
	modemenu->items[i].is_selected = 0;
	modemenu->items[i].items = 0;
	modemenu->items[i].action = act_set_active_mode;
	modemenu->items[i].action_arg = &(modemenu->items[1]);
	modemenu->items[i].parent = modemenu;

	// exit item
	i++;
	modemenu->items[i].id = i;
	modemenu->items[i].size = 0;
	modemenu->items[i].heading = STR_BACK;
	modemenu->items[i].items = 0;
	modemenu->items[i].action = act_goto_parent;
	act_goto_parent_arg * post_args_back = malloc(sizeof(act_goto_parent_arg)); // never deleted
	post_args_back->m = modemenu;
	post_args_back->s = s;
	modemenu->items[i].action_arg = post_args_back;
}

void init_motors_menu(menu * motorsmenu, menu_supervisor *s) {
	motorsmenu->id = ID_MOTORSMENU;
	motorsmenu->size = 4;
	motorsmenu->heading = STR_MAIN_ITEM_POST_MOTORS;
	motorsmenu->items = malloc(TOTAL_POSTS*sizeof(menu));
	motorsmenu->action = 0;
	motorsmenu->is_selected = 0;
	act_submenu_arg * args = malloc(sizeof(act_submenu_arg)); // will never be destroyed

	args->m = motorsmenu;
	args->s = s;

	motorsmenu->action = act_submenu_motor;
	motorsmenu->action_arg = args;

	int i=0;

	// esq500 item
	motorsmenu->items[i].id = i;
	motorsmenu->items[i].size = 0;
	motorsmenu->items[i].heading = STR_DRIVER_ESQ500;
	motorsmenu->items[i].items = 0;
	motorsmenu->items[i].action = 0;
	motorsmenu->items[i].is_selected = 0;
	motorsmenu->items[i].items = 0;
	motorsmenu->items[i].action = act_set_active_motor;
	motorsmenu->items[i].action_arg = &(motorsmenu->items[0]);
	motorsmenu->items[i].parent = motorsmenu;

	// esq770 item
	i++;
	motorsmenu->items[i].id = i;
	motorsmenu->items[i].size = 0;
	motorsmenu->items[i].heading = STR_DRIVER_ESQ770;
	motorsmenu->items[i].items = 0;
	motorsmenu->items[i].action = 0;
	motorsmenu->items[i].is_selected = 0;
	motorsmenu->items[i].items = 0;
	motorsmenu->items[i].action = act_set_active_motor;
	motorsmenu->items[i].action_arg = &(motorsmenu->items[1]);
	motorsmenu->items[i].parent = motorsmenu;

	// ae200h item
	i++;
	motorsmenu->items[i].id = i;
	motorsmenu->items[i].size = 0;
	motorsmenu->items[i].heading = STR_DRIVER_AE200H;
	motorsmenu->items[i].items = 0;
	motorsmenu->items[i].action = 0;
	motorsmenu->items[i].is_selected = 0;
	motorsmenu->items[i].items = 0;
	motorsmenu->items[i].action = act_set_active_motor;
	motorsmenu->items[i].action_arg = &(motorsmenu->items[2]);
	motorsmenu->items[i].parent = motorsmenu;

	// exit item
	i++;
	motorsmenu->items[i].id = i;
	motorsmenu->items[i].size = 0;
	motorsmenu->items[i].heading = STR_BACK;
	motorsmenu->items[i].items = 0;
	motorsmenu->items[i].action = act_goto_parent;
	act_goto_parent_arg * post_args_back = malloc(sizeof(act_goto_parent_arg)); // never deleted
	post_args_back->m = motorsmenu;
	post_args_back->s = s;
	motorsmenu->items[i].action_arg = post_args_back;
}


void display_menu_item(menu * menu_item, int8_t start_item, int8_t cursor) {
	SSD1306_Clear();
	SSD1306_GotoXY(0, 0);
	SSD1306_Puts(menu_item->heading, &Font_7x10, 1);

	int8_t cur_line = 1; // we're starting not from 0
	for(int8_t i=start_item; i < menu_item->size && cur_line < MAX_MENU_LINES; i++) {
		SSD1306_GotoXY(8, 10 * cur_line);
		if (cursor != i) {
			SSD1306_Puts(menu_item->items[i].heading, &Font_7x10, 1);
			if (menu_item->items[i].is_selected) {
				SSD1306_GotoXY(0, 10 * cur_line);
				SSD1306_Puts(">", &Font_7x10, 1);
			}
		} else {
			SSD1306_DrawFilledRectangle(0, 10 * cur_line-1, 127, 10, 1);
			SSD1306_Puts(menu_item->items[i].heading, &Font_7x10, 0);
			if (menu_item->items[i].is_selected) {
				SSD1306_GotoXY(0, 10 * cur_line);
				SSD1306_Puts(">", &Font_7x10, 0);
			}
		}
		cur_line++;
	}
	SSD1306_UpdateScreen();
}

void act_submenu_pos(void *arg) {
	act_submenu_arg * args = (act_submenu_arg *)arg;
	uint8_t post_num = get_post_num();
	// Let's make just one of them selected
	for (uint8_t i = 0;i<args->m->size; i++) {
	if (i + 1 == post_num) {
		args->m->items[i].is_selected = 1;
	} else {
		args->m->items[i].is_selected = 0;
	}

	act_goto_menu_aux(args->s, args->m, post_num - 1);
}
}

void act_exit(void *arg) {
	menu_supervisor * s = (menu_supervisor *)arg;
	s->exit = 1;
}

void act_test_relays(void * arg) {
	const char * cmd = "RUN T60|1/900/900|2/850/850|3/800/800|4/750/750|5/725/725|6/700/700|7/650/650|8/630/630|9/600/600|10/550/550|11/500/500|;";
	app_process_cmd(cmd);
}

void act_goto_parent(void * arg) {
	act_goto_parent_arg * args = (act_goto_parent_arg *)arg;

	int8_t desired_position = 0;

	// Let's find required position
	for (int8_t i=0;i<args->m->parent->size;i++) {
		if (&(args->m->parent->items[i]) == args->m) {
			desired_position = i;
		}
	}

	act_goto_menu_aux(args->s, args->m->parent, desired_position);
}

void act_set_active(void * arg) {
	menu * post_item = (menu *) arg;
	menu * parent = post_item->parent;
	for (uint8_t i=0;i<parent->size;i++) {
		if (i+1 == post_item->id) {
			// we need to make it selected
			parent->items[i].is_selected = 1;
			set_post_num(post_item->id);
		} else {
			parent->items[i].is_selected = 0;
		}
	}
}
void act_goto_menu_aux(menu_supervisor *s, menu *m, int8_t desired_position) {
	s->cursor = desired_position;
	s->start_item = desired_position;

	// Let's position it properly on the screen
	// e.g. if the last element is selected we won't see just one element
	// on the screen
	if (m->size < MAX_MENU_LINES) {
		s->start_item = 0;
	} else {
		int8_t max_start_item = 1 + m->size - MAX_MENU_LINES;
		if (s->start_item > max_start_item) {
			s->start_item = max_start_item;
		}
	}

	s->cur_item = m;
}

void act_set_active_motor(void * arg) {
	menu * item = (menu *) arg;
	menu * parent = item->parent;
	for (uint8_t i=0;i<parent->size;i++) {
		if (i == item->id) {
			// we need to make it selected
			parent->items[i].is_selected = 1;
			set_motor(item->id);
		} else {
			parent->items[i].is_selected = 0;
		}
	}
}

void act_set_active_mode(void * arg) {
	menu * item = (menu *) arg;
	menu * parent = item->parent;
	for (uint8_t i=0;i<parent->size;i++) {
		if (i == item->id) {
			// we need to make it selected
			parent->items[i].is_selected = 1;
			set_connection_mode(item->id);
		} else {
			parent->items[i].is_selected = 0;
		}
	}
}

void act_submenu_motor(void *arg) {
	act_submenu_arg * args = (act_submenu_arg *)arg;
	uint8_t motor = get_motor();
	// Let's make just one of them selected
	for (uint8_t i = 0;i<args->m->size; i++) {
	if (i == motor) {
		args->m->items[i].is_selected = 1;
	} else {
		args->m->items[i].is_selected = 0;
	}

	act_goto_menu_aux(args->s, args->m, motor);
}
}

void act_submenu_mode(void *arg) {
	act_submenu_arg * args = (act_submenu_arg *)arg;
	uint8_t mode = get_connection_mode();
	// Let's make just one of them selected
	for (uint8_t i = 0;i<args->m->size; i++) {
	if (i == mode) {
		args->m->items[i].is_selected = 1;
	} else {
		args->m->items[i].is_selected = 0;
	}

	act_goto_menu_aux(args->s, args->m, mode);
}
}
