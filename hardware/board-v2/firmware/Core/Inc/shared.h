/*
 * shared.h
 *
 *  Created on: Feb 20, 2021
 *      Author: Roman Fedyashov
 */

#ifndef INC_SHARED_H_
#define INC_SHARED_H_

#define MAX_CMD_BUF 193

#define RS485_DISABLE 0
#define RS485_TX 1
#define RS485_RX 2

#define MOTOR_NOT_ANSWERING 0
#define MOTOR_WRONG_ANSWERS 1
#define MOTOR_OK            2

#define RELAY_MODE_ALWAYS_ON 1
#define RELAY_MODE_ALWAYS_OFF 2
#define RELAY_MODE_SWITCHING 3

#define RELAY_NOW_ON 1
#define RELAY_NOW_OFF 2

uint32_t flash_read();
void flash_write(uint32_t data);

#endif /* INC_SHARED_H_ */
