#define MAX_SEND_BUF 32
#define MAX_DELAY_MS 100



#ifndef __STM32F0xx_IT_H
#define __STM32F0xx_IT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "cmsis_os.h"
#include "semphr.h"



void NMI_Handler(void);
void HardFault_Handler(void);
void TIM6_IRQHandler(void);
void USART1_IRQHandler(void);
void USART2_IRQHandler(void);
void set_rs_mode(uint8_t new_mode);

uint8_t _send_byte_from_buffer();

typedef struct uart_user_friendly_s {
	USART_TypeDef * port;
	//for sending
	char send_cycle_buffer[MAX_SEND_BUF + 2];
	uint8_t send_cursor;
	uint8_t remaining_bytes;
	void (*event_handler)(uint8_t symbol);
	uint8_t can_do_both_dirs;
} uart_user_friendly;

void usart_sending_off(USART_TypeDef * usart);
void usart_sending_on(USART_TypeDef * usart);
void default_usart_handler(uart_user_friendly * user_uart);
void init_usarts();
void set_rs_mode(uint8_t new_mode);
void set_incoming_byte_handler(uint8_t port_num, void (*event_handler)(uint8_t symbol));
uint8_t send_bytes(uint8_t port_num, const char * new_data, uint8_t size);

#ifdef __cplusplus
}
#endif

#endif


