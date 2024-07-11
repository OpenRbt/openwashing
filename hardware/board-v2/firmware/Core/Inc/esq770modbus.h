/*
 * esq770modbus.h
 *
 *  Created on: Aug 12, 2024
 *      Author: roman
 */

#ifndef INC_ESQ770MODBUS_H_
#define INC_ESQ770MODBUS_H_

char * start_motor_cmd_770();

char * set_motor_speed_cmd_770(uint8_t percent);

char * stop_motor_cmd_770();

char * read_motor_info_cmd_770();

void esq_vars_init_770();


#endif /* INC_ESQ770MODBUS_H_ */
