/*
 * esq500modbus.h
 *
 *  Created on: Feb 22, 2021
 *      Author: Roman Fedyashov
 *      This simple lib is about creating modbus requests for ESQ500
 *      and parsing their answers
 *      The code can be used in SINGLE thread only!!!
 */

#ifndef INC_ESQ500MODBUS_H_
#define INC_ESQ500MODBUS_H_

#include <stdint.h>

#define ESQ_CODE_OK 0
#define ESQ_CODE_ERR 1

#define ESQ_STATUS_UNKNOWN 0
#define ESQ_STATUS_RUNNING 1
#define ESQ_STATUS_STOPPED 2



typedef struct motor_parsed_info_s {
	uint8_t status;
} motor_parsed_info;

char * start_motor_cmd();
uint8_t parse_start_motor_answer(const char * answer);

char * set_motor_speed_cmd(uint8_t percent);
uint8_t parse_set_motor_speed_answer(const char * answer);

char * stop_motor_cmd();
uint8_t parse_stop_motor_cmd_answer(const char * answer);

char * read_motor_info_cmd();
uint8_t parse_motor_info_answer(const char * answer, motor_parsed_info * status);

void esq_vars_init();

void set_cmd_crc(const char *cmd, uint8_t size, uint16_t * crc_sum);

#endif /* INC_ESQ500MODBUS_H_ */
