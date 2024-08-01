/*
 * esq500modbus.c
 *
 *  Created on: Feb 22, 2021
 *      Author: Roman Fedyashov
 *  The code can be used in SINGLE thread only!!!
 *  Each method doubles
 */


#include "esq500modbus.h"
#define DEVICE_ADDR 0x01

#define WRITE_COMMAND 0x06
#define READ_COMMAND 0x03

// read info command
char cmd_motor_info[8];

// start motor 2 commands
char cmd_start_motor[8];
char cmd_set_speed[8];

// stop motor command
char cmd_stop_motor[8];

void set_cmd_crc(const char *cmd, uint8_t size, uint16_t * crc_sum) {
	*crc_sum = 0xffff;
	uint16_t polynom = 0xa001;
	for (uint8_t i = 0; i<size;i++) {
		uint8_t cur_element = (uint8_t)cmd[i];
		*crc_sum = *crc_sum ^ cur_element;
		for (uint8_t j = 0;j<8;j++) {
			if(*crc_sum & 0x0001) {
				*crc_sum >>= 1;
				*crc_sum = *crc_sum ^ polynom;
			} else {
				*crc_sum >>= 1;
			}
		}
	}
}

void esq_vars_init(){
	// header
	cmd_motor_info[0] = DEVICE_ADDR;
	cmd_motor_info[1] = READ_COMMAND;
	// address
	cmd_motor_info[2] = 0x1e;
	cmd_motor_info[3] = 0x02;
	// value
	cmd_motor_info[4] = 0x00;
	cmd_motor_info[5] = 0x01;
	// crc
	set_cmd_crc(cmd_motor_info, 6, (uint16_t *)&cmd_motor_info[6]);

	//header
	cmd_start_motor[0] = DEVICE_ADDR;
	cmd_start_motor[1] = WRITE_COMMAND;
	// address
	cmd_start_motor[2] = 0x1e;
	cmd_start_motor[3] = 0x00;
	// value
	cmd_start_motor[4] = 0x00;
	cmd_start_motor[5] = 0x05;
	// crc
	set_cmd_crc(cmd_start_motor, 6, (uint16_t *)&cmd_start_motor[6]);

	// header
	cmd_set_speed[0] = DEVICE_ADDR;
	cmd_set_speed[1] = WRITE_COMMAND;
	// address
	cmd_set_speed[2] = 0x1e;
	cmd_set_speed[3] = 0x01;
	// value
	cmd_set_speed[4] = 0x13; //1388 is 5000, which is 50%
	cmd_set_speed[5] = 0x88;
	// crc
	set_cmd_crc(cmd_set_speed, 6, (uint16_t *)&cmd_set_speed[6]);

	// header
	cmd_stop_motor[0] = DEVICE_ADDR;
	cmd_stop_motor[1] = WRITE_COMMAND;
	// address
	cmd_stop_motor[2] = 0x1e;
	cmd_stop_motor[3] = 0x00;
	// value
	cmd_stop_motor[4] = 0x00;
	cmd_stop_motor[5] = 0x06;
	// crc
	set_cmd_crc(cmd_stop_motor, 6, (uint16_t *)&cmd_stop_motor[6]);
}


// returns a command to start_motor starts motor
char * start_motor_cmd() {
	return cmd_start_motor;
}

uint8_t parse_start_motor_answer(const char * answer) {
	return 0;
}

// returns a command to start_motor starts motor at specified percent
char * set_motor_speed_cmd(uint8_t percent) {
	if(percent>100) {
		percent = 100;
	}
	uint16_t big_percent = percent * 100;
	char * val = (char*)&big_percent;
	cmd_set_speed[5] = val[0];
	cmd_set_speed[4] = val[1];
	set_cmd_crc(cmd_set_speed, 6, (uint16_t *)&cmd_set_speed[6]);
	return cmd_set_speed;
}

uint8_t parse_set_motor_speed_answer(const char * answer) {
	return 0;
}

// read_motor_info collects info from the motor
char * read_motor_info_cmd() {
	return cmd_motor_info;
}

uint8_t parse_motor_info_answer(const char * answer, motor_parsed_info * status) {
	return 0;
}

// stop motor stops the motor
char * stop_motor_cmd() {
	return cmd_stop_motor;
}

uint8_t parse_stop_motor_cmd_answer(const char * answer) {
	return 0;
}


