/*
 * app.c
 *
 *  Created on: Feb 13, 2021
 *      Author: Roman Fedyashov
 */



#include "app.h"
#include "shared.h"
#include "esq500modbus.h"
//#include "stm32f0xx_hal_flash.h"

#define MODE_SPACE 0
#define MODE_TIMER 10
#define MODE_ALL_RELAY_DEFAULT 20
#define MODE_MOTOR 30
#define MODE_RELAY 40
#define MODE_RELAY_NUM 41
#define MODE_RELAY_ONTIME 42
#define MODE_RELAY_OFFTIME 43
#define MODE_ERROR 100
#define MODE_ERROR_MOTOR_PERCENT_TOO_BIG 101
#define MODE_ERROR_TIMEOUT_TOO_BIG 102
#define MODE_ERROR_RELAY_DEFAULT 103
#define MODE_ERROR_RELAY_NUM 104
#define MODE_ERROR_BADSYMBOLS_RELAY 105
#define MODE_ERROR_BIG_ONTIME 106
#define MODE_ERROR_BIG_OFFTIME 107
#define MODE_ERROR_PANIC 108


char uid[32] = {0,};
int current_state = ST_NOT_INITIALIZED;
uint32_t last_ping = 0;
void (*app_send_data)(const char *, int) = 0;
void (*rs485_write_data)(const char *, int);
uint32_t (*get_tick)(void) = 0;

// functions to work with relays
void (*app_turn_on)(uint8_t) = 0;
void (*app_turn_off)(uint8_t) = 0;
void (*just_sleep_for_ms)(uint32_t) = 0;

void set_sleep_func(void (*new_just_sleep_for_ms)(uint32_t)) {
	just_sleep_for_ms = new_just_sleep_for_ms;
}

char post_num_str[3] = {0,};
hw_settings current_settings;

// variables to control hardware relays
relay_hardware_status hardware_relay[11];
uint32_t timeout_ms;

// variables to control motor_speed
motor_full_status motor_status;

void zero_hw_relays_vars(){
	for(int i=0;i<11;i++) {
		hardware_relay[i].current_position = RELAY_NOW_OFF;
		hardware_relay[i].last_switching_time = 0;
		hardware_relay[i].relay_working_mode = RELAY_MODE_ALWAYS_OFF;
	}
	timeout_ms = 0;
}

void app_set_desired_motor_speed(uint8_t percent) {
	motor_status.is_in_use = 1;
	if(percent > 100) {
		percent = 100;
	}
	if (!percent) {
		motor_status.desired_status = MOTOR_STATUS_STOPPED;
		motor_status.desired_speed = 0;
	} else {
		motor_status.desired_status = MOTOR_STATUS_RUNNING;
		motor_status.desired_speed = percent;
	}
}

uint8_t status_by_error(uint8_t err) {
	if (err == ERROR_SHORT_ANSWER) {
		return MOTOR_NOT_ANSWERING;
	} else if (err == ERROR_WRONG_ANSWER) {
		return MOTOR_WRONG_ANSWERS;
	} else {
		return MOTOR_OK;
	}
}

uint8_t get_motor_communication_status() {
	return motor_status.communication_status;
}
// motor_loop returns an error
uint8_t motor_loop() {
	// Let it start once per 1 ms
	motor_status.inactive_loops++;

	if (motor_status.desired_status == MOTOR_STATUS_STOPPED) {
		if(motor_status.current_status == MOTOR_STATUS_RUNNING) {
			motor_status.inactive_loops = 0;
			uint8_t err = app_stop_motor();
			if (!err) {
				motor_status.current_status = MOTOR_STATUS_STOPPED;
			}
			motor_status.communication_status = status_by_error(err);
		}
	} else {
		uint8_t desired_speed = motor_status.desired_speed;
		if (motor_status.current_speed != desired_speed) {
			motor_status.inactive_loops = 0;
			uint8_t err = app_set_motor_speed(desired_speed);
			motor_status.communication_status = status_by_error(err);
			if (err) return err;
			motor_status.current_speed = desired_speed;
		}
		if (motor_status.current_status == MOTOR_STATUS_STOPPED) {
			motor_status.inactive_loops = 0;
			uint8_t err = app_start_motor();
			motor_status.communication_status = status_by_error(err);
			if (err) {
				return err;
			}
		}
	}
	if (motor_status.inactive_loops > 2000) {
		motor_status.inactive_loops = 0;
		if (motor_status.is_in_use) {
			if(motor_status.desired_status == MOTOR_STATUS_STOPPED) {
				uint8_t err = app_stop_motor();
				motor_status.communication_status = status_by_error(err);
			} else {
				uint8_t err = app_set_motor_speed(motor_status.desired_speed);
				motor_status.communication_status = status_by_error(err);
				if (err) return err;
				err = app_start_motor();
				motor_status.communication_status = status_by_error(err);
			}
		} else {
			uint8_t err = app_get_motor_info();
			motor_status.communication_status = status_by_error(err);
		}
	}
	return 0;
}

void send_rs_485_command(const char *cmd, uint8_t size) {
	motor_status.rx_buffer_cursor = 0;
	rs485_write_data(cmd, size);
}

uint8_t are_equal(const char * src1, const char * src2, uint8_t size) {
	for (int i=0; i<size;i++) {
		if (src1[i] != src2[i]) {
			return 0;
		}
	}
	return 1;
}

uint8_t send_generic_cmd(const char *cur_cmd) {
	send_rs_485_command(cur_cmd, 8);
	uint8_t max_delay = 50;
	for (;max_delay > 0 && motor_status.rx_buffer_cursor<8;) {
		max_delay--;
		just_sleep_for_ms(1);
	}
	if(motor_status.rx_buffer_cursor < 8) {
		return ERROR_SHORT_ANSWER;
	}
	if(!are_equal(cur_cmd, motor_status.rx_buffer, 8)) {
		return ERROR_WRONG_ANSWER;
	}
	return 0;
}

uint8_t send_read_cmd(const char *cur_cmd) {
	send_rs_485_command(cur_cmd, 8);
	uint8_t max_delay = 50;
	for (;max_delay > 0 && motor_status.rx_buffer_cursor<6;) {
		max_delay--;
		just_sleep_for_ms(1);
	}
	if(motor_status.rx_buffer_cursor < 6) {
		return ERROR_SHORT_ANSWER;
	}
	// We need to parse read info, right now we can just compare first two bytes
	if(!are_equal(cur_cmd, motor_status.rx_buffer, 2)) {
		return ERROR_WRONG_ANSWER;
	}
	return 0;
}

// returns error
uint8_t app_stop_motor(){
	char * cur_cmd = stop_motor_cmd();
	return send_generic_cmd(cur_cmd);
}

// returns error
uint8_t app_set_motor_speed(uint8_t desired_speed) {
	char * cur_cmd = set_motor_speed_cmd(desired_speed);
	return send_generic_cmd(cur_cmd);
}

// returns error
uint8_t app_start_motor() {
	char * cur_cmd = start_motor_cmd();
	return send_generic_cmd(cur_cmd);
}

uint8_t app_get_motor_info() {
	char * cur_cmd = read_motor_info_cmd();
	return send_read_cmd(cur_cmd);
}

void app_push_motor_byte(uint8_t key) {
	if (motor_status.rx_buffer_cursor == 0 && key == 0) {
		// we do not need to add leading zero bytes
		return;
	}
	motor_status.rx_buffer[motor_status.rx_buffer_cursor] = key;
	motor_status.rx_buffer_cursor++;
	if(motor_status.rx_buffer_cursor > MOTOR_RX_SIZE) {
		motor_status.rx_buffer_cursor = MOTOR_RX_SIZE;
	}
}

void zero_motor_settings() {
	motor_status.is_in_use = 0;
	motor_status.desired_speed = 0;
	motor_status.current_speed = 0;
	motor_status.current_status = MOTOR_STATUS_STOPPED;
	motor_status.desired_status = MOTOR_STATUS_STOPPED;
	motor_status.rx_buffer_cursor = 0;
	motor_status.inactive_loops = 0;
	motor_status.communication_status = MOTOR_NOT_ANSWERING;
}

void app_init() {
	recover_settings((uint32_t *)&current_settings);
	zero_hw_relays_vars();
	zero_motor_settings();
	esq_vars_init();
}

void set_turn_on_func(void (*new_turn_on)(uint8_t)) {
	app_turn_on = new_turn_on;
}

void set_turn_off_func(void (*new_turn_off)(uint8_t)) {
	app_turn_off = new_turn_off;
}

void app_update_relays() {
	// let's check timeout first
	if (timeout_ms) {
		if(timeout_ms == 1) {
			zero_hw_relays_vars();
			for (int i = 0;i<11;i++) {
				app_turn_off(i);
			}
			if(motor_status.is_in_use) {
				app_set_desired_motor_speed(0);
			}
		} else {
			timeout_ms--;
		}
	}
	for(int i=0;i<11;i++) {
		relay_hardware_status * cur_hw_relay = &hardware_relay[i];
		if(cur_hw_relay->relay_working_mode == RELAY_MODE_ALWAYS_OFF) {
			if(cur_hw_relay->current_position == RELAY_NOW_ON) {
				app_turn_off(i);
				cur_hw_relay->current_position = RELAY_NOW_OFF;
			}
		} else if (cur_hw_relay->relay_working_mode == RELAY_MODE_ALWAYS_ON) {
			if(cur_hw_relay->current_position == RELAY_NOW_OFF) {
				app_turn_on(i);
				cur_hw_relay->current_position = RELAY_NOW_ON;
			}
		} else {
			// switching :(
			if(cur_hw_relay->current_position == RELAY_NOW_ON) {
				//if it is on we need to turn it off when its time is gone
				uint32_t time_to_turn_off = cur_hw_relay->last_switching_time + cur_hw_relay->ontime;
				uint32_t current_time = get_tick();
				if (current_time > time_to_turn_off) {
					cur_hw_relay->last_switching_time = current_time;
					cur_hw_relay->current_position = RELAY_NOW_OFF;
					app_turn_off(i);
				}
			} else {
				//if it is off we need to turn it on when required
				uint32_t time_to_turn_on = cur_hw_relay->last_switching_time + cur_hw_relay->offtime;
				uint32_t current_time = get_tick();
				if (current_time > time_to_turn_on) {
					cur_hw_relay->last_switching_time = current_time;
					cur_hw_relay->current_position = RELAY_NOW_ON;
					app_turn_on(i);
				}
			}
		}

	}
}

void _update_post_num() {
	uint8_t post_number = current_settings.main_post_number;
	post_num_str[2] = 0;
	uint8_t high = post_number / 10;
	post_num_str[0] = high + '0';
	uint8_t low = post_number % 10;
	post_num_str[1] = low + '0';
}

void recover_settings(uint32_t * obj) {
	*obj = flash_read();
	if (current_settings.main_post_number<1) {
		current_settings.main_post_number = 1;
		write_settings((uint32_t *)&current_settings);
	}
	if (current_settings.main_post_number>99) {
		current_settings.main_post_number = 99;
		write_settings((uint32_t *)&current_settings);
	}
	_update_post_num();
}

void write_settings(uint32_t * obj) {
	flash_write(*obj);
}

char * get_post_num_str() {
	return post_num_str;
}

uint8_t get_post_num() {
	return current_settings.main_post_number;
}
void set_post_num(uint8_t new_post_num) {
	uint8_t old_post_num = current_settings.main_post_number;

	current_settings.main_post_number = new_post_num;
	if(current_settings.main_post_number < MIN_POST_NUM) current_settings.main_post_number = MAX_POST_NUM;
	if(current_settings.main_post_number > MAX_POST_NUM) current_settings.main_post_number = MIN_POST_NUM;
	_update_post_num();
	if (old_post_num != current_settings.main_post_number) {
		write_settings((uint32_t *)&current_settings);
	}
}

void inc_post_num() {
	set_post_num(current_settings.main_post_number + 1);
}

void dec_post_num() {
	set_post_num(current_settings.main_post_number - 1);
}

void set_rs485_write(void (*new_rs485_write_data)(const char *, int)) {
	rs485_write_data = new_rs485_write_data;
}

void set_app_send_data(void (*new_app_send_data)(const char *, int)) {
	app_send_data = new_app_send_data;
}

void set_tick_func(uint32_t (*new_get_tick)(void)) {
	get_tick = new_get_tick;
}
char int_to_hexchar(uint8_t key) {
	if (key < 9) return '0' + key;
	return 55 + key;
}

void _copy_hex_from_byte_to_buf(uint8_t sym, char * target) {
	uint8_t high = sym >> 4;
	uint8_t low = sym & 0b00001111;
	target[0] = int_to_hexchar(high);
	target[1] = int_to_hexchar(low);
}

char * get_uid() {
	if (!uid[0]) {
		uint8_t *idBase = (uint8_t*)(UID_BASE);
		for (uint8_t i = 0;i < 12;i++) {
			uint8_t sym = idBase[i];
			_copy_hex_from_byte_to_buf(sym, &uid[i * 2]);
		}
		uid[24] = 0;
	}
	return uid;
}

void set_state(uint8_t new_state) {
	current_state = new_state;
}

void app_process_cmd(const char * cmd) {
	if(get_tick) {
		last_ping = get_tick();
	}
	if(cmd[0]=='U' && cmd[1]=='I' && cmd[2] == 'D') {
		app_decode_uid(&cmd[3]);
		return;
	}
	if(cmd[0]=='S' && cmd[1]=='E' && cmd[2]=='T') {
		app_decode_set(&cmd[3]);
		return;
	}
	if(cmd[0]=='P' && cmd[1]=='I' && cmd[2] == 'N' && cmd[3] == 'G') {
		app_decode_ping(&cmd[4]);
		return;
	}
	if(cmd[0]=='R' && cmd[1]=='U' && cmd[2]=='N') {
		app_decode_run(&cmd[3]);
	}
}
void app_decode_ping(const char *cmd) {
	// right now we don't even parse additional data
	if(app_send_data) {
		app_send_data(post_num_str, 3);
	}
}

uint8_t process_char(char key, char * buf, uint8_t * cur, uint8_t mode, relay_reader_config * r_config)  {
	if(mode == MODE_SPACE) {
		if (key == 'M') {
			return MODE_MOTOR;
		}
		if (key == 'T') {
			return MODE_TIMER;
		}
		if (key == 'A') {
			return MODE_ALL_RELAY_DEFAULT;
		}
		if (key>='0' && key <='9') {
			buf[0] = key;
			*cur = 1;
			return MODE_RELAY;
		}
		return MODE_SPACE;
	}
	if(mode == MODE_MOTOR) {
		if (key == '|') {
			return MODE_SPACE;
		}
		if(key<'0' || key >'9') {
			return MODE_ERROR;
		}
		r_config->motor_mentioned = 1;
		if (r_config->motor_percent>20) {
			return MODE_ERROR_MOTOR_PERCENT_TOO_BIG;
		}
		r_config->motor_percent *= 10;
		r_config->motor_percent += (key-'0');
		return MODE_MOTOR;
	}
	if(mode == MODE_TIMER) {
		if (key == '|') {
			return MODE_SPACE;
		}
		if(key<'0' || key >'9') {
			return MODE_ERROR;
		}
		r_config->timeout_sec_specified = 1;

		r_config->timeout_sec*=10;
		r_config->timeout_sec+= (key-'0');
		if (r_config->timeout_sec > 3600) {
			return MODE_ERROR_TIMEOUT_TOO_BIG;
		}
		return MODE_TIMER;
	}
	if (mode == MODE_ALL_RELAY_DEFAULT) {
		if (key == '|') {
			return MODE_SPACE;
		}
		if (key == '+') {
			r_config->default_relay_state = DEFAULT_RELAY_ON;
			return MODE_ALL_RELAY_DEFAULT;
		}
		if (key == '-') {
			r_config->default_relay_state = DEFAULT_RELAY_OFF;
			return MODE_ALL_RELAY_DEFAULT;
		}
		return MODE_ERROR_RELAY_DEFAULT;
	}
	if (mode == MODE_RELAY) {
		if (key == '|' || key == '/') {
			uint8_t relay_num = 0;
			for (int i=0; i<*cur; i++) {
				relay_num *=10;
				relay_num += (buf[i] - '0');
			}
			*cur = 0;
			if(relay_num < 1 || relay_num > 11) {
				return MODE_ERROR_RELAY_NUM;
			}
			relay_num--;
			r_config->relays[relay_num].used = 1;
			r_config->last_changed_relay = relay_num;
			if(key == '|') return MODE_SPACE;
			return MODE_RELAY_ONTIME;
		}
		if (key < '0' || key > '9') {
			return MODE_ERROR_BADSYMBOLS_RELAY;
		}
		buf[*cur] = key;
		*cur = *cur + 1;
		if (*cur > 2) {
			return MODE_ERROR_RELAY_NUM;
		}
		return MODE_RELAY;
	}
	if (mode == MODE_RELAY_ONTIME) {
		if (key == '|') {
			return MODE_ERROR_BADSYMBOLS_RELAY;
		}
		if (key == '/')	{
			return MODE_RELAY_OFFTIME;
		}
		if (key < '0' || key > '9') {
			return MODE_ERROR_BADSYMBOLS_RELAY;
		}
		uint8_t relay_num = r_config->last_changed_relay;
		if (r_config->relays[relay_num].ontime > 200) {
			return MODE_ERROR_BIG_ONTIME;
		}
		r_config->relays[relay_num].ontime*=10;
		r_config->relays[relay_num].ontime += (key - '0');
		return MODE_RELAY_ONTIME;
	}
	if (mode == MODE_RELAY_OFFTIME) {
		if (key == '|') {
			return MODE_SPACE;
		}
		if (key < '0' || key > '9') {
			return MODE_ERROR_BADSYMBOLS_RELAY;
		}
		uint8_t relay_num = r_config->last_changed_relay;
		if (r_config->relays[relay_num].offtime > 200) {
			return MODE_ERROR_BIG_OFFTIME;
		}
		r_config->relays[relay_num].offtime*=10;
		r_config->relays[relay_num].offtime += (key - '0');
		return MODE_RELAY_OFFTIME;
	}
	return MODE_ERROR_PANIC;
}

void init_relay_config(relay_reader_config * r_config) {
	for (int i=0;i<11;i++) {
		r_config->relays[i].ontime = 0;
		r_config->relays[i].offtime = 0;
		r_config->relays[i].used = 0;
	}
	r_config->last_changed_relay = 0;
	r_config->motor_mentioned = 0;
	r_config->motor_percent = 0;
	r_config->timeout_sec = 0;
	r_config->timeout_sec_specified = 0;
	r_config->default_relay_state = DEFAULT_RELAY_NO_DEFAULT;
}

void app_decode_run(const char *cmd) {
	relay_reader_config r_config;
	init_relay_config(&r_config);

	// State machine to parse a string
	char buf[16];
	int i = 0;
	uint8_t cur = 0;
	uint8_t mode = 0;

	for (;cmd[i]!=';' && i<MAX_CMD_BUF-1;) {
		mode = process_char(cmd[i], buf, &cur, mode, &r_config);
		i++;
		if (mode < MODE_ERROR) continue;
		if (mode == MODE_ERROR_MOTOR_PERCENT_TOO_BIG) {
			app_send_data("motor % big", 11);
			return;
		}
		if (mode == MODE_ERROR_TIMEOUT_TOO_BIG) {
			app_send_data("timeout big", 11);
			return;
		}
		if (mode == MODE_ERROR_RELAY_DEFAULT) {
			app_send_data("A uses + or -", 13);
			return;
		}
		if (mode == MODE_ERROR_RELAY_NUM) {
			app_send_data("RELAY MUST BE 1 TO 11", 21);
			return;
		}
		if (mode == MODE_ERROR_BADSYMBOLS_RELAY) {
			app_send_data("BAD SYM IN RELAY", 16);
			return;
		}
		if (mode == MODE_ERROR_BIG_ONTIME) {
			app_send_data("BIG ONTIME", 10);
			return;
		}
		if (mode == MODE_ERROR_BIG_OFFTIME) {
			app_send_data("BIG OFFTIME", 11);
			return;
		}
		if (mode >= MODE_ERROR) {
			app_send_data("PANIC", 4);
			return;
		}
	}
	app_send_data("OK", 2);
	app_apply_relays(&r_config);
}
void app_apply_relays(relay_reader_config  * new_relays) {
	for (int i=0;i<11;i++) {
		// need to update?
		if(new_relays->relays[i].used) {
			uint32_t ontime = new_relays->relays[i].ontime;
			uint32_t offtime = new_relays->relays[i].offtime;
			if ( ontime > 0 && offtime > 0) {
				hardware_relay[i].relay_working_mode = RELAY_MODE_SWITCHING;
				hardware_relay[i].ontime = ontime;
				hardware_relay[i].offtime = offtime;
			} else if (ontime == 0 && offtime > 0) {
				hardware_relay[i].relay_working_mode = RELAY_MODE_ALWAYS_OFF;
			} else {
				hardware_relay[i].relay_working_mode = RELAY_MODE_ALWAYS_ON;
			}
		} else if (new_relays->default_relay_state == DEFAULT_RELAY_OFF) {
			hardware_relay[i].relay_working_mode = RELAY_MODE_ALWAYS_OFF;
		} else if (new_relays->default_relay_state == DEFAULT_RELAY_ON) {
			hardware_relay[i].relay_working_mode = RELAY_MODE_ALWAYS_ON;
		}
	}
	if (new_relays->timeout_sec_specified) {
		timeout_ms = 1 + new_relays->timeout_sec * 1000;
	}
	// TODO UPDATE MOTOR
	if (new_relays->motor_mentioned) {
		app_set_desired_motor_speed(new_relays->motor_percent);
	}
}

void app_decode_uid(const char *cmd) {
	char uid_answer[30];
	cp("UID ", uid_answer, 0);
	char *uid = get_uid();
	cp(uid, &uid_answer[4], 24);
	cp(";", &uid_answer[28], 0);

	app_send_data(uid_answer, 29);
	// now we don't parse it even
}

void app_decode_set(const char *cmd) {
	uint8_t post_num = 0;
	uint8_t digits = 0;
	for(int i=0;i<4 && digits<2;i++) {
		if(cmd[i]>='0' && cmd[i]<='9') {
			digits++;
			uint8_t digit = cmd[i] - '0';
			post_num = post_num * 10;
			post_num += digit;
		} else {
			if(digits!=0) break;
		}
	}
	set_post_num(post_num);
	app_send_data("OK", 2);
}

void cp(char * from, char *to, uint8_t size) {
	if (!size) {
		int i;
		for(i=0;from[i] != 0 && i< MAX_CMD_BUF;i++) {
			to[i] = from[i];
		}
		to[i] = 0;
	} else {
		for (int i=0;i<size;i++) {
			to[i] = from[i];
		}
	}
}

uint8_t get_state() {
	if (get_tick) {
		uint32_t cur_tick = get_tick();
		uint32_t time_passed = cur_tick - last_ping;
		if(time_passed > 1000) {
			current_state = ST_WAITING_FOR_CONNECTION;
		} else {
			current_state = ST_WORKING;
		}
	}
	return current_state;
}
