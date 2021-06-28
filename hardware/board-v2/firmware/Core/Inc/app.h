/*
 * app.h
 *
 *  Created on: Feb 13, 2021
 *      Author: Roman Fedyashov
 */

#ifndef INC_APP_H_
#define INC_APP_H_

#define ST_NOT_INITIALIZED 0
#define ST_WAITING_FOR_CONNECTION 1
#define ST_WORKING 2
#define ST_CONNECTION_LOST 3
#define ST_UNKNOWN e 100

// Please do not change these defines, they are just FYI
#define MIN_POST_NUM 1
#define MAX_POST_NUM 99

#define MOTOR_STATUS_STOPPED 0
#define MOTOR_STATUS_RUNNING 1

#define ERROR_SHORT_ANSWER 1
#define ERROR_WRONG_ANSWER 2

#include "stm32f070xb.h"

#define MOTOR_RX_SIZE 32
typedef struct motor_full_status_s {
	uint8_t is_in_use; // indicates whether motor is in use;
	uint8_t current_status; // running or stopped
	uint8_t current_speed; // 0 to 100
	uint8_t desired_status; // running or stopped
	uint8_t desired_speed; //0 to 100
	char rx_buffer[MOTOR_RX_SIZE + 1];
	uint8_t rx_buffer_cursor;
	uint16_t inactive_loops;
	uint8_t communication_status;
} motor_full_status;

typedef struct hw_settings_s {
	uint8_t posts_supported; // 1 or 2 is a valid value here. Invalid value means 1
	uint8_t main_post_number; // position of the first post
	uint8_t second_post_number; // position of the second post
	uint8_t dummy;
} hw_settings;


typedef struct relay_reader_s {
	uint16_t ontime;
	uint16_t offtime;
	uint8_t used;
} relay_reader;

#define DEFAULT_RELAY_NO_DEFAULT 0
#define DEFAULT_RELAY_OFF 1
#define DEFAULT_RELAY_ON 2

typedef struct relay_reader_config_s {
	uint8_t last_changed_relay;
	uint8_t default_relay_state;
	uint8_t motor_mentioned;
	uint8_t motor_percent;
	uint8_t timeout_sec_specified;
	uint32_t timeout_sec;
	relay_reader relays[11];
} relay_reader_config;

typedef struct relay_hardware_status_s {
	uint8_t relay_working_mode;
	uint8_t current_position;
	uint32_t last_switching_time;
	uint32_t ontime;
	uint32_t offtime;
} relay_hardware_status;

void app_init();
void recover_settings(uint32_t * obj);
void write_settings(uint32_t * obj);
char * get_post_num_str();
uint8_t get_post_num();
void set_post_num(uint8_t new_post_num);
void inc_post_num();
void dec_post_num();

char * get_uid();
char int_to_hexchar(uint8_t key);
void set_state(uint8_t new_state);
uint8_t get_state();
void set_incoming_usb_handler();
void app_process_cmd(const char * received_cmd);
void app_decode_uid(const char * cmd);
void app_decode_set(const char * cmd);
void app_decode_ping(const char * cmd);
void app_decode_run(const char *cmd);

// returns error
uint8_t app_stop_motor();
// returns error
uint8_t app_start_motor();
// returns error
uint8_t app_set_motor_speed(uint8_t desired_speed);
// returns error
uint8_t app_get_motor_info();

void app_push_motor_byte(uint8_t key);
void set_rs485_write(void (*new_rs485_write_data)(const char *, int));

uint8_t get_motor_communication_status();

void app_set_desired_motor_speed(uint8_t percent);
void set_app_send_data(void (*new_app_send_data)(const char *, int));
void set_tick_func(uint32_t (*new_get_tick)(void));
void set_sleep_func(void (*new_just_sleep_for_ms)(uint32_t));

void set_turn_on_func(void (*new_turn_on)(uint8_t));
void set_turn_off_func(void (*new_turn_off)(uint8_t));

void app_apply_relays(relay_reader_config  * new_relays);
void app_update_relays();
void zero_hw_relays_vars();
uint8_t motor_loop();

motor_full_status * get_motor_status();

void cp(char * from, char *to, uint8_t size);
#endif /* INC_APP_H_ */
