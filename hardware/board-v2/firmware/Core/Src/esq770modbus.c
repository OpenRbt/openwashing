/*
 * esq770modbus.c
 *
 *  Created on: Aug 12, 2024
 *      Author: roman
 */


#include "esq500modbus.h"
#include "esq770modbus.h"
#define DEVICE_ADDR_770 0x01

#define WRITE_COMMAND_770 0x06
#define READ_COMMAND_770 0x03

// read info command
char cmd_motor_info_770[8];

// start motor 2 commands
char cmd_start_motor_770[8];
char cmd_set_speed_770[8];

// stop motor command
char cmd_stop_motor_770[8];


void esq_vars_init_770(){
	// header
	cmd_motor_info_770[0] = DEVICE_ADDR_770;
	cmd_motor_info_770[1] = READ_COMMAND_770;
	// address
	cmd_motor_info_770[2] = 0x60;
	cmd_motor_info_770[3] = 0x00;
	// value
	cmd_motor_info_770[4] = 0x00;
	cmd_motor_info_770[5] = 0x01;
	// crc
	set_cmd_crc(cmd_motor_info_770, 6, (uint16_t *)&cmd_motor_info_770[6]);

	//header
	cmd_start_motor_770[0] = DEVICE_ADDR_770;
	cmd_start_motor_770[1] = WRITE_COMMAND_770;
	// address
	cmd_start_motor_770[2] = 0x20;
	cmd_start_motor_770[3] = 0x00;
	// value
	cmd_start_motor_770[4] = 0x00;
	cmd_start_motor_770[5] = 0x01;
	// crc
	set_cmd_crc(cmd_start_motor_770, 6, (uint16_t *)&cmd_start_motor_770[6]);

	// header
	cmd_set_speed_770[0] = DEVICE_ADDR_770;
	cmd_set_speed_770[1] = WRITE_COMMAND_770;
	// address
	cmd_set_speed_770[2] = 0x30;
	cmd_set_speed_770[3] = 0x00;
	// value
	cmd_set_speed_770[4] = 0x09; //25Hz
	cmd_set_speed_770[5] = 0xc4;
	// crc
	set_cmd_crc(cmd_set_speed_770, 6, (uint16_t *)&cmd_set_speed_770[6]);

	// header
	cmd_stop_motor_770[0] = DEVICE_ADDR_770;
	cmd_stop_motor_770[1] = WRITE_COMMAND_770;
	// address
	cmd_stop_motor_770[2] = 0x20;
	cmd_stop_motor_770[3] = 0x00;
	// value
	cmd_stop_motor_770[4] = 0x00;
	cmd_stop_motor_770[5] = 0x05;
	// crc
	set_cmd_crc(cmd_stop_motor_770, 6, (uint16_t *)&cmd_stop_motor_770[6]);
}


// returns a command to start_motor starts motor
char * start_motor_cmd_770() {
	return cmd_start_motor_770;
}

// returns a command to start_motor starts motor at specified percent
char * set_motor_speed_cmd_770(uint8_t percent) {
	if(percent>100) {
		percent = 100;
	}
	uint16_t big_percent = percent * 100;
	char * val = (char*)&big_percent;
	cmd_set_speed_770[5] = val[0];
	cmd_set_speed_770[4] = val[1];
	set_cmd_crc(cmd_set_speed_770, 6, (uint16_t *)&cmd_set_speed_770[6]);
	return cmd_set_speed_770;
}

// read_motor_info collects info from the motor
char * read_motor_info_cmd_770() {
	return cmd_motor_info_770;
}

// stop motor stops the motor
char * stop_motor_cmd_770() {
	return cmd_stop_motor_770;
}

